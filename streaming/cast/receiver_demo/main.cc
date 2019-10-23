#include <array>
#include <chrono>
#include <thread>

#include "platform/api/logging.h"
#include "platform/api/time.h"
#include "platform/api/udp_socket.h"
#include "platform/base/error.h"
#include "platform/base/ip_address.h"
#include "platform/impl/socket_handle_waiter_thread.h"
#include "platform/impl/task_runner.h"
#include "platform/impl/udp_socket_reader_posix.h"
#include "streaming/cast/constants.h"
#include "streaming/cast/environment.h"
#include "streaming/cast/receiver.h"
#include "streaming/cast/receiver_packet_router.h"
#include "streaming/cast/ssrc.h"

namespace {

////////////////////////////////////////////////////////////////////////////////
// Receiver Configuration
//
// The values defined here are constants that correspond to the Sender Demo app.
// In a production environment, these should ABSOLUTELY NOT be fixed! Instead a
// sender↔receiver OFFER/ANSWER exchange should establish them.

// The UDP socket port receiving packets from the Sender.
constexpr int kCastStreamingPort = 2344;

// The SSRC's should be randomly generated using either
// openscreen::cast_streaming::GenerateSsrc(), or a similar heuristic.
using openscreen::cast_streaming::Ssrc;
constexpr Ssrc kDemoAudioSenderSsrc = 1;
constexpr Ssrc kDemoAudioReceiverSsrc = 2;
constexpr Ssrc kDemoVideoSenderSsrc = 50001;
constexpr Ssrc kDemoVideoReceiverSsrc = 50002;

// In a production environment, this would be set to the sampling rate of the
// audio capture.
constexpr int kDemoAudioRtpTimebase = 48000;

// Per the Cast Streaming spec, this is always 90 kHz. See kVideoTimebase in
// constants.h.
constexpr int kDemoVideoRtpTimebase =
    static_cast<int>(openscreen::cast_streaming::kVideoTimebase::den);

// In a production environment, this would start-out at some initial value
// appropriate to the networking environment, and then be adjusted by the
// application as: 1) the TYPE of the content changes (interactive, low-latency
// versus smooth, higher-latency buffered video watching); and 2) the networking
// environment reliability changes.
constexpr std::chrono::milliseconds kDemoTargetPlayoutDelay{400};

// In a production environment, these should be generated from a
// cryptographically-secure RNG source.
constexpr std::array<uint8_t, 16> kDemoAudioAesKey{
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
constexpr std::array<uint8_t, 16> kDemoAudioCastIvMask{
    0xf0, 0xe0, 0xd0, 0xc0, 0xb0, 0xa0, 0x90, 0x80,
    0x70, 0x60, 0x50, 0x40, 0x30, 0x20, 0x10, 0x00};
constexpr std::array<uint8_t, 16> kDemoVideoAesKey{
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f};
constexpr std::array<uint8_t, 16> kDemoVideoCastIvMask{
    0xf1, 0xe1, 0xd1, 0xc1, 0xb1, 0xa1, 0x91, 0x81,
    0x71, 0x61, 0x51, 0x41, 0x31, 0x21, 0x11, 0x01};

// End of Receiver Configuration.
////////////////////////////////////////////////////////////////////////////////

}  // namespace

int main(int argc, const char* argv[]) {
  // Platform setup for this standalone demo app.
  openscreen::platform::LogInit(nullptr /* stdout */);
  openscreen::platform::SetLogLevel(openscreen::platform::LogLevel::kInfo);
  const auto now_function = &openscreen::platform::Clock::now;
  openscreen::platform::TaskRunnerImpl task_runner(now_function);
  openscreen::platform::SocketHandleWaiterThread socket_handle_waiter_thread;
  openscreen::platform::UdpSocketReaderPosix udp_socket_reader(
      socket_handle_waiter_thread.socket_handle_waiter());

  // Create the Environment that holds the required injected dependencies
  // (clock, task runner) used throughout the system, and owns the UDP socket
  // over which all communication occurs with the Sender.
  const openscreen::IPEndpoint receive_endpoint{openscreen::IPAddress(),
                                                kCastStreamingPort};
  openscreen::cast_streaming::Environment env(now_function, &task_runner,
                                              receive_endpoint);

  // Create the packet router that allows both the Audio Receiver and the Video
  // Receiver to share the same UDP socket.
  openscreen::cast_streaming::ReceiverPacketRouter packet_router(&env);

  // Create the two Receivers.
  openscreen::cast_streaming::Receiver audio_receiver(
      &env, &packet_router, kDemoAudioSenderSsrc, kDemoAudioReceiverSsrc,
      kDemoAudioRtpTimebase, kDemoTargetPlayoutDelay, kDemoAudioAesKey,
      kDemoAudioCastIvMask);
  openscreen::cast_streaming::Receiver video_receiver(
      &env, &packet_router, kDemoVideoSenderSsrc, kDemoVideoReceiverSsrc,
      kDemoVideoRtpTimebase, kDemoTargetPlayoutDelay, kDemoVideoAesKey,
      kDemoVideoCastIvMask);

  OSP_LOG_INFO << "Awaiting first Cast Streaming packet at "
               << env.GetBoundLocalEndpoint() << "...";

  // Create/Initialize the Audio Player and Video Player, which are responsible
  // for decoding and playing out the received media.
  /*
    // TODO: SDL implementation to play in a window on the local desktop.
    SdlAudioPlayer audio_player(&audio_receiver);
    SdlVideoPlayer video_player(&video_receiver);

    // Note: See Receiver class-level comments for player code example.
  */

  // Run the event loop until an exit is requested (e.g., the video player GUI
  // window is closed, a SIGTERM is intercepted, or whatever other appropriate
  // user indication that shutdown is requested).
  task_runner.RunUntilStopped();
  return 0;
}
