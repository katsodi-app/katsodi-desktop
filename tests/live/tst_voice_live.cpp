// Live voice check against a real LiveKit server (not part of ctest).
// Run through native-desktop/tools/voice-live-check.mjs, which mints two
// short-lived tokens for a throwaway room and passes them in the
// environment: KATSODI_LK_URL, KATSODI_LK_TOKEN_A, KATSODI_LK_TOKEN_B.
//
// Two engines in one process join the same room. It proves connection,
// microphone publication through the platform audio device module, mute
// state propagation, deafen, device listing and clean disconnect. It does
// not prove that a person hears the other side: that needs ears.
#include "media/livekit/LiveKitVoiceEngine.h"

#include <QElapsedTimer>
#include <QSignalSpy>
#include <QTest>

using namespace katsodi;

namespace {
bool hasPeer(const QList<VoicePeer> &peers, const QString &identity, int microphone /* -1 any */)
{
    for (const VoicePeer &peer : peers)
        if (peer.identity == identity && (microphone < 0 || peer.microphone == (microphone == 1)))
            return true;
    return false;
}
} // namespace

class VoiceLive : public QObject {
    Q_OBJECT
private slots:
    void twoEnginesTalk()
    {
        const QString url = qEnvironmentVariable("KATSODI_LK_URL");
        const QString tokenA = qEnvironmentVariable("KATSODI_LK_TOKEN_A");
        const QString tokenB = qEnvironmentVariable("KATSODI_LK_TOKEN_B");
        if (url.isEmpty() || tokenA.isEmpty() || tokenB.isEmpty())
            QSKIP("Run through tools/voice-live-check.mjs");

        LiveKitVoiceEngine a, b;
        QList<VoicePeer> seenByB;
        connect(&b, &VoiceEngine::peersChanged, this, [&](quint64, const QList<VoicePeer> &peers) { seenByB = peers; });
        QList<AudioDevice> inputs, outputs;
        connect(&a, &VoiceEngine::devicesChanged, this, [&](const QList<AudioDevice> &in, const QList<AudioDevice> &out) {
            inputs = in;
            outputs = out;
        });
        QSignalSpy aConnected(&a, &VoiceEngine::connected), bConnected(&b, &VoiceEngine::connected);
        QSignalSpy aFailed(&a, &VoiceEngine::connectFailed), bFailed(&b, &VoiceEngine::connectFailed);
        QSignalSpy aMic(&a, &VoiceEngine::microphoneChanged), aMicFailed(&a, &VoiceEngine::microphoneFailed);

        QElapsedTimer timer;
        timer.start();
        a.connectCall(1, url, tokenA);
        QTRY_VERIFY_WITH_TIMEOUT(aConnected.count() == 1 || aFailed.count() == 1, 30000);
        QCOMPARE(aFailed.count(), 0);
        qInfo("A connected in %lld ms", timer.restart());
        b.connectCall(1, url, tokenB);
        QTRY_VERIFY_WITH_TIMEOUT(bConnected.count() == 1 || bFailed.count() == 1, 30000);
        QCOMPARE(bFailed.count(), 0);
        qInfo("B connected in %lld ms", timer.restart());

        QTRY_VERIFY_WITH_TIMEOUT(hasPeer(seenByB, QStringLiteral("native-check-a"), -1), 20000);
        QVERIFY(hasPeer(seenByB, QStringLiteral("native-check-a"), 0)); // listen-first: no microphone

        a.setMicrophone(true);
        QTRY_VERIFY_WITH_TIMEOUT(aMic.count() >= 1 || aMicFailed.count() >= 1, 20000);
        QCOMPARE(aMicFailed.count(), 0);
        QTRY_VERIFY_WITH_TIMEOUT(hasPeer(seenByB, QStringLiteral("native-check-a"), 1), 20000);
        qInfo("B sees A's microphone after %lld ms", timer.restart());

        a.setMicrophone(false);
        QTRY_VERIFY_WITH_TIMEOUT(hasPeer(seenByB, QStringLiteral("native-check-a"), 0), 20000);
        qInfo("B sees A muted after %lld ms", timer.restart());
        a.setMicrophone(true);
        QTRY_VERIFY_WITH_TIMEOUT(hasPeer(seenByB, QStringLiteral("native-check-a"), 1), 20000);

        b.setDeafened(true);
        QTest::qWait(1500);
        b.setDeafened(false);

        a.refreshDevices();
        QTRY_VERIFY_WITH_TIMEOUT(!inputs.isEmpty() || !outputs.isEmpty(), 10000);
        qInfo("devices: %lld microphones, %lld speakers", qsizetype(inputs.size()), qsizetype(outputs.size()));

        a.disconnectCall();
        QTRY_VERIFY_WITH_TIMEOUT(!hasPeer(seenByB, QStringLiteral("native-check-a"), -1), 20000);
        qInfo("B saw A leave after %lld ms", timer.restart());
        b.disconnectCall();
    }
};

QTEST_GUILESS_MAIN(VoiceLive)
#include "tst_voice_live.moc"
