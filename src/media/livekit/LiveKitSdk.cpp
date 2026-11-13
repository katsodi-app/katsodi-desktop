#include "media/livekit/LiveKitSdk.h"

#include <livekit/livekit.h>

#include <mutex>

namespace katsodi::livekitsdk {
namespace {
std::mutex g_mutex;
int g_users = 0;
} // namespace

void acquire()
{
    std::lock_guard lock(g_mutex);
    if (g_users++ == 0)
        livekit::initialize(livekit::LogLevel::Warn);
}

void release()
{
    std::lock_guard lock(g_mutex);
    if (--g_users == 0)
        livekit::shutdown();
}

} // namespace katsodi::livekitsdk
