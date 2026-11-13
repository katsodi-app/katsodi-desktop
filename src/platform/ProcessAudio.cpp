#include "platform/ProcessAudio.h"

#include <QMetaObject>

#ifdef Q_OS_WIN
#include <windows.h>
#include <audioclient.h>
#include <audioclientactivationparams.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>
#include <wrl/implements.h>
#endif

#include <chrono>
#include <cstring>

namespace katsodi {

#ifdef Q_OS_WIN
namespace {
using Microsoft::WRL::ComPtr;

// ActivateAudioInterfaceAsync calls back on an MTA thread; the handler must
// be agile (FtmBase) and only signals an event the capture thread waits on.
class Activation : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
                                                       Microsoft::WRL::FtmBase, IActivateAudioInterfaceCompletionHandler> {
public:
    Activation() { done = CreateEventW(nullptr, TRUE, FALSE, nullptr); }
    ~Activation() override { CloseHandle(done); }
    STDMETHOD(ActivateCompleted)(IActivateAudioInterfaceAsyncOperation *operation) override
    {
        HRESULT activated = E_FAIL;
        ComPtr<IUnknown> unknown;
        if (SUCCEEDED(operation->GetActivateResult(&activated, &unknown)) && SUCCEEDED(activated))
            unknown.As(&client);
        result = activated;
        SetEvent(done);
        return S_OK;
    }
    HANDLE done = nullptr;
    HRESULT result = E_FAIL;
    ComPtr<IAudioClient> client;
};
} // namespace
#endif

ProcessAudio::ProcessAudio(QObject *parent) : QObject(parent) {}

ProcessAudio::~ProcessAudio() { stop(); }

bool ProcessAudio::start(quint32 processId, bool includeTree, Sink sink)
{
    stop();
#ifdef Q_OS_WIN
    m_stop = false;
    m_started = 0;
    m_thread = std::thread([this, processId, includeTree, sink = std::move(sink)]() mutable {
        run(processId, includeTree, std::move(sink));
    });
    // Activation is quick; wait for its outcome so the caller can tell the user.
    for (int i = 0; i < 300 && m_started == 0; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    if (m_started != 1) {
        stop();
        return false;
    }
    return true;
#else
    Q_UNUSED(processId);
    Q_UNUSED(includeTree);
    Q_UNUSED(sink);
    return false;
#endif
}

void ProcessAudio::stop()
{
    m_stop = true;
    if (m_thread.joinable())
        m_thread.join();
}

void ProcessAudio::run(quint32 processId, bool includeTree, Sink sink)
{
#ifdef Q_OS_WIN
    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) {
        m_started = -1;
        return;
    }
    AUDIOCLIENT_ACTIVATION_PARAMS params{};
    params.ActivationType = AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
    params.ProcessLoopbackParams.TargetProcessId = processId;
    params.ProcessLoopbackParams.ProcessLoopbackMode =
        includeTree ? PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE : PROCESS_LOOPBACK_MODE_EXCLUDE_TARGET_PROCESS_TREE;
    PROPVARIANT activation{};
    activation.vt = VT_BLOB;
    activation.blob.cbSize = sizeof(params);
    activation.blob.pBlobData = reinterpret_cast<BYTE *>(&params);

    auto handler = Microsoft::WRL::Make<Activation>();
    ComPtr<IActivateAudioInterfaceAsyncOperation> operation;
    ComPtr<IAudioCaptureClient> capture;
    HANDLE ready = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    bool ok = SUCCEEDED(ActivateAudioInterfaceAsync(VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK, __uuidof(IAudioClient),
                                                    &activation, handler.Get(), &operation))
              && WaitForSingleObject(handler->done, 3000) == WAIT_OBJECT_0 && SUCCEEDED(handler->result)
              && handler->client;
    if (ok) {
        // The loopback device has no mix format of its own: ask for ours.
        WAVEFORMATEX format{};
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = channels;
        format.nSamplesPerSec = sampleRate;
        format.wBitsPerSample = 16;
        format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
        format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
        ok = SUCCEEDED(handler->client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                                   AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK
                                                       | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM,
                                                   200 * 10000, 0, &format, nullptr))
             && SUCCEEDED(handler->client->SetEventHandle(ready))
             && SUCCEEDED(handler->client->GetService(__uuidof(IAudioCaptureClient), &capture))
             && SUCCEEDED(handler->client->Start());
    }
    m_started = ok ? 1 : -1;
    if (ok) {
        std::vector<std::int16_t> chunk;
        chunk.reserve(chunkFrames * channels);
        auto last = std::chrono::steady_clock::now();
        while (!m_stop) {
            const DWORD waited = WaitForSingleObject(ready, 100);
            if (m_stop)
                break;
            UINT32 packet = 0;
            bool got = false;
            while (SUCCEEDED(capture->GetNextPacketSize(&packet)) && packet > 0) {
                BYTE *data = nullptr;
                UINT32 frames = 0;
                DWORD flags = 0;
                if (FAILED(capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr)))
                    break;
                const std::size_t values = std::size_t(frames) * channels;
                const std::size_t start = chunk.size();
                chunk.resize(start + values);
                if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
                    std::memset(chunk.data() + start, 0, values * sizeof(std::int16_t));
                else
                    std::memcpy(chunk.data() + start, data, values * sizeof(std::int16_t));
                capture->ReleaseBuffer(frames);
                got = true;
                while (chunk.size() >= std::size_t(chunkFrames) * channels) {
                    std::vector<std::int16_t> out(chunk.begin(), chunk.begin() + chunkFrames * channels);
                    chunk.erase(chunk.begin(), chunk.begin() + chunkFrames * channels);
                    sink(std::move(out));
                }
            }
            // A silent application delivers no packets at all: keep time with zeros.
            const auto now = std::chrono::steady_clock::now();
            if (got) {
                last = now;
            } else if (waited == WAIT_TIMEOUT || now - last > std::chrono::milliseconds(40)) {
                while (now - last >= std::chrono::milliseconds(20)) {
                    sink(std::vector<std::int16_t>(std::size_t(chunkFrames) * channels, 0));
                    last += std::chrono::milliseconds(20);
                }
            }
        }
        handler->client->Stop();
    }
    capture.Reset();
    CloseHandle(ready);
    CoUninitialize();
    if (ok && !m_stop)
        QMetaObject::invokeMethod(this, &ProcessAudio::failed, Qt::QueuedConnection);
#else
    Q_UNUSED(processId);
    Q_UNUSED(includeTree);
    Q_UNUSED(sink);
    m_started = -1;
#endif
}

} // namespace katsodi
