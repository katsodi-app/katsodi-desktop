#pragma once

namespace katsodi::livekitsdk {

// livekit::initialize/shutdown are process-wide: the SDK stays up while any
// engine (voice, screen) exists and shuts down with the last one.
void acquire();
void release();

} // namespace katsodi::livekitsdk
