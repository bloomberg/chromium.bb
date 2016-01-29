// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/task_runner_util.h"
#include "base/thread_task_runner_handle.h"
#include "jingle/glue/thread_wrapper.h"
#include "net/base/test_data_directory.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/client/audio_player.h"
#include "remoting/client/chromoting_client.h"
#include "remoting/client/client_context.h"
#include "remoting/client/client_user_interface.h"
#include "remoting/codec/video_decoder_verbatim.h"
#include "remoting/codec/video_decoder_vpx.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/fake_desktop_environment.h"
#include "remoting/protocol/frame_consumer.h"
#include "remoting/protocol/jingle_session_manager.h"
#include "remoting/protocol/me2me_host_authenticator_factory.h"
#include "remoting/protocol/negotiating_client_authenticator.h"
#include "remoting/protocol/session_config.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/protocol/video_frame_pump.h"
#include "remoting/protocol/video_renderer.h"
#include "remoting/signaling/fake_signal_strategy.h"
#include "remoting/test/cyclic_frame_generator.h"
#include "remoting/test/fake_network_dispatcher.h"
#include "remoting/test/fake_port_allocator.h"
#include "remoting/test/fake_socket_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

using protocol::ChannelConfig;

namespace {

const char kHostJid[] = "host_jid@example.com/host";
const char kHostOwner[] = "jane.doe@example.com";
const char kClientJid[] = "jane.doe@example.com/client";

struct NetworkPerformanceParams {
  NetworkPerformanceParams(int bandwidth,
                           int max_buffers,
                           double latency_average_ms,
                           double latency_stddev_ms,
                           double out_of_order_rate)
      : bandwidth(bandwidth),
        max_buffers(max_buffers),
        latency_average(base::TimeDelta::FromMillisecondsD(latency_average_ms)),
        latency_stddev(base::TimeDelta::FromMillisecondsD(latency_stddev_ms)),
        out_of_order_rate(out_of_order_rate) {}

  int bandwidth;
  int max_buffers;
  base::TimeDelta latency_average;
  base::TimeDelta latency_stddev;
  double out_of_order_rate;
};

class FakeCursorShapeStub : public protocol::CursorShapeStub {
 public:
  FakeCursorShapeStub() {}
  ~FakeCursorShapeStub() override {}

  // protocol::CursorShapeStub interface.
  void SetCursorShape(const protocol::CursorShapeInfo& cursor_shape) override{};
};

scoped_ptr<webrtc::DesktopFrame> DoDecodeFrame(
    VideoDecoder* decoder,
    VideoPacket* packet,
    scoped_ptr<webrtc::DesktopFrame> frame) {
  if (!decoder->DecodePacket(*packet, frame.get()))
    frame.reset();
  return frame;
}

}  // namespace

class ProtocolPerfTest
    : public testing::Test,
      public testing::WithParamInterface<NetworkPerformanceParams>,
      public ClientUserInterface,
      public protocol::VideoRenderer,
      public protocol::VideoStub,
      public protocol::FrameConsumer,
      public HostStatusObserver {
 public:
  ProtocolPerfTest()
      : host_thread_("host"),
        capture_thread_("capture"),
        encode_thread_("encode"),
        decode_thread_("decode") {
    protocol::VideoFramePump::EnableTimestampsForTests();
    host_thread_.StartWithOptions(
        base::Thread::Options(base::MessageLoop::TYPE_IO, 0));
    capture_thread_.Start();
    encode_thread_.Start();
    decode_thread_.Start();
  }

  virtual ~ProtocolPerfTest() {
    host_thread_.task_runner()->DeleteSoon(FROM_HERE, host_.release());
    host_thread_.task_runner()->DeleteSoon(FROM_HERE,
                                           host_signaling_.release());
    message_loop_.RunUntilIdle();
  }

  // ClientUserInterface interface.
  void OnConnectionState(protocol::ConnectionToHost::State state,
                         protocol::ErrorCode error) override {
    if (state == protocol::ConnectionToHost::CONNECTED) {
      client_connected_ = true;
      if (host_connected_)
        connecting_loop_->Quit();
    }
  }
  void OnConnectionReady(bool ready) override {}
  void OnRouteChanged(const std::string& channel_name,
                      const protocol::TransportRoute& route) override {}
  void SetCapabilities(const std::string& capabilities) override {}
  void SetPairingResponse(
      const protocol::PairingResponse& pairing_response) override {}
  void DeliverHostMessage(const protocol::ExtensionMessage& message) override {}
  protocol::ClipboardStub* GetClipboardStub() override { return nullptr; }
  protocol::CursorShapeStub* GetCursorShapeStub() override {
    return &cursor_shape_stub_;
  }

  // VideoRenderer interface.
  void OnSessionConfig(const protocol::SessionConfig& config) override {}
  protocol::VideoStub* GetVideoStub() override { return this; }
  protocol::FrameConsumer* GetFrameConsumer() override { return this; }

  // protocol::VideoStub interface.
  void ProcessVideoPacket(scoped_ptr<VideoPacket> packet,
                          const base::Closure& done) override {
    if (packet->data().empty()) {
      // Ignore keep-alive packets
      done.Run();
      return;
    }

    if (packet->format().has_screen_width() &&
        packet->format().has_screen_height()) {
      frame_size_.set(packet->format().screen_width(),
                      packet->format().screen_height());
    }

    scoped_ptr<webrtc::DesktopFrame> frame(
        new webrtc::BasicDesktopFrame(frame_size_));
    base::PostTaskAndReplyWithResult(
        decode_thread_.task_runner().get(), FROM_HERE,
        base::Bind(&DoDecodeFrame, video_decoder_.get(), packet.get(),
                   base::Passed(&frame)),
        base::Bind(&ProtocolPerfTest::OnFrameDecoded, base::Unretained(this),
                   base::Passed(&packet), done));
  }

  void OnFrameDecoded(scoped_ptr<VideoPacket> packet,
                      const base::Closure& done,
                      scoped_ptr<webrtc::DesktopFrame> frame) {
    last_video_packet_ = std::move(packet);
    DrawFrame(std::move(frame), done);
  }

  // protocol::FrameConsumer interface.
  scoped_ptr<webrtc::DesktopFrame> AllocateFrame(
      const webrtc::DesktopSize& size) override {
    return make_scoped_ptr(new webrtc::BasicDesktopFrame(size));
  }

  void DrawFrame(scoped_ptr<webrtc::DesktopFrame> frame,
                 const base::Closure& done) override {
    last_video_frame_ = std::move(frame);
    if (!on_frame_task_.is_null())
      on_frame_task_.Run();
    if (!done.is_null())
      done.Run();
  }

  protocol::FrameConsumer::PixelFormat GetPixelFormat() override {
    return FORMAT_BGRA;
  }

  // HostStatusObserver interface.
  void OnClientConnected(const std::string& jid) override {
    message_loop_.PostTask(
        FROM_HERE,
        base::Bind(&ProtocolPerfTest::OnHostConnectedMainThread,
                   base::Unretained(this)));
  }

 protected:
  void WaitConnected() {
    client_connected_ = false;
    host_connected_ = false;

    connecting_loop_.reset(new base::RunLoop());
    connecting_loop_->Run();

    ASSERT_TRUE(client_connected_ && host_connected_);
  }

  void OnHostConnectedMainThread() {
    host_connected_ = true;
    if (client_connected_)
      connecting_loop_->Quit();
  }

  scoped_ptr<webrtc::DesktopFrame> ReceiveFrame() {
    last_video_frame_.reset();

    waiting_frames_loop_.reset(new base::RunLoop());
    on_frame_task_ = waiting_frames_loop_->QuitClosure();
    waiting_frames_loop_->Run();

    EXPECT_TRUE(last_video_frame_);
    return std::move(last_video_frame_);
  }

  void ReceiveFrameAndGetLatency(base::TimeDelta* latency) {
    last_video_packet_.reset();

    ReceiveFrame();

    if (latency) {
      base::TimeTicks timestamp =
          base::TimeTicks::FromInternalValue(last_video_packet_->timestamp());
      *latency = base::TimeTicks::Now() - timestamp;
    }
  }

  void ReceiveMultipleFramesAndGetMaxLatency(int frames,
                                             base::TimeDelta* max_latency) {
    if (max_latency)
      *max_latency = base::TimeDelta();

    for (int i = 0; i < frames; ++i) {
      base::TimeDelta latency;

      ReceiveFrameAndGetLatency(&latency);

      if (max_latency && latency > *max_latency) {
        *max_latency = latency;
      }
    }
  }

  // Creates test host and client and starts connection between them. Caller
  // should call WaitConnected() to wait until connection is established. The
  // host is started on |host_thread_| while the client works on the main
  // thread.
  void StartHostAndClient(bool use_webrtc,
                          protocol::ChannelConfig::Codec video_codec) {
    fake_network_dispatcher_ =  new FakeNetworkDispatcher();

    client_signaling_.reset(new FakeSignalStrategy(kClientJid));

    jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();

    protocol_config_ = protocol::CandidateSessionConfig::CreateDefault();
    protocol_config_->DisableAudioChannel();
    protocol_config_->mutable_video_configs()->clear();
    protocol_config_->mutable_video_configs()->push_back(
        protocol::ChannelConfig(
            protocol::ChannelConfig::TRANSPORT_STREAM, 2, video_codec));
    protocol_config_->set_webrtc_supported(use_webrtc);
    protocol_config_->set_ice_supported(!use_webrtc);

    switch (video_codec) {
      case ChannelConfig::CODEC_VERBATIM:
        video_decoder_.reset(new VideoDecoderVerbatim());
        break;
      case ChannelConfig::CODEC_VP8:
        video_decoder_ = VideoDecoderVpx::CreateForVP8();
        break;
      default:
        NOTREACHED();
    }

    host_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&ProtocolPerfTest::StartHost, base::Unretained(this)));
  }

  void StartHost() {
    DCHECK(host_thread_.task_runner()->BelongsToCurrentThread());

    jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();

    host_signaling_.reset(new FakeSignalStrategy(kHostJid));
    host_signaling_->ConnectTo(client_signaling_.get());

    protocol::NetworkSettings network_settings(
        protocol::NetworkSettings::NAT_TRAVERSAL_OUTGOING);

    scoped_ptr<FakePortAllocatorFactory> port_allocator_factory(
        new FakePortAllocatorFactory(fake_network_dispatcher_));
    port_allocator_factory->socket_factory()->SetBandwidth(
        GetParam().bandwidth, GetParam().max_buffers);
    port_allocator_factory->socket_factory()->SetLatency(
        GetParam().latency_average, GetParam().latency_stddev);
    port_allocator_factory->socket_factory()->set_out_of_order_rate(
        GetParam().out_of_order_rate);
    scoped_refptr<protocol::TransportContext> transport_context(
        new protocol::TransportContext(
            host_signaling_.get(), std::move(port_allocator_factory),
            network_settings, protocol::TransportRole::SERVER));
    scoped_ptr<protocol::SessionManager> session_manager(
        new protocol::JingleSessionManager(host_signaling_.get()));
    session_manager->set_protocol_config(protocol_config_->Clone());

    // Encoder runs on a separate thread, main thread is used for everything
    // else.
    host_.reset(new ChromotingHost(
        &desktop_environment_factory_, std::move(session_manager),
        transport_context, host_thread_.task_runner(),
        host_thread_.task_runner(), capture_thread_.task_runner(),
        encode_thread_.task_runner(), host_thread_.task_runner(),
        host_thread_.task_runner()));

    base::FilePath certs_dir(net::GetTestCertsDirectory());

    std::string host_cert;
    ASSERT_TRUE(base::ReadFileToString(
        certs_dir.AppendASCII("unittest.selfsigned.der"), &host_cert));

    base::FilePath key_path = certs_dir.AppendASCII("unittest.key.bin");
    std::string key_string;
    ASSERT_TRUE(base::ReadFileToString(key_path, &key_string));
    std::string key_base64;
    base::Base64Encode(key_string, &key_base64);
    scoped_refptr<RsaKeyPair> key_pair = RsaKeyPair::FromString(key_base64);
    ASSERT_TRUE(key_pair.get());

    protocol::SharedSecretHash host_secret;
    host_secret.hash_function = protocol::AuthenticationMethod::NONE;
    host_secret.value = "123456";
    scoped_ptr<protocol::AuthenticatorFactory> auth_factory =
        protocol::Me2MeHostAuthenticatorFactory::CreateWithSharedSecret(
            true, kHostOwner, host_cert, key_pair, host_secret, nullptr);
    host_->SetAuthenticatorFactory(std::move(auth_factory));

    host_->AddStatusObserver(this);
    host_->Start(kHostOwner);

    message_loop_.PostTask(FROM_HERE,
                           base::Bind(&ProtocolPerfTest::StartClientAfterHost,
                                      base::Unretained(this)));
  }

  void StartClientAfterHost() {
    client_signaling_->ConnectTo(host_signaling_.get());

    protocol::NetworkSettings network_settings(
        protocol::NetworkSettings::NAT_TRAVERSAL_OUTGOING);

    // Initialize client.
    client_context_.reset(
        new ClientContext(base::ThreadTaskRunnerHandle::Get()));

    scoped_ptr<FakePortAllocatorFactory> port_allocator_factory(
        new FakePortAllocatorFactory(fake_network_dispatcher_));
    port_allocator_factory->socket_factory()->SetBandwidth(
        GetParam().bandwidth, GetParam().max_buffers);
    port_allocator_factory->socket_factory()->SetLatency(
        GetParam().latency_average, GetParam().latency_stddev);
    port_allocator_factory->socket_factory()->set_out_of_order_rate(
        GetParam().out_of_order_rate);
    scoped_refptr<protocol::TransportContext> transport_context(
        new protocol::TransportContext(
            host_signaling_.get(), std::move(port_allocator_factory),
            network_settings, protocol::TransportRole::CLIENT));

    std::vector<protocol::AuthenticationMethod> auth_methods;
    auth_methods.push_back(protocol::AuthenticationMethod::Spake2(
        protocol::AuthenticationMethod::NONE));
    scoped_ptr<protocol::Authenticator> client_authenticator(
        new protocol::NegotiatingClientAuthenticator(
            std::string(),  // client_pairing_id
            std::string(),  // client_pairing_secret
            std::string(),  // authentication_tag
            base::Bind(&ProtocolPerfTest::FetchPin, base::Unretained(this)),
            nullptr, auth_methods));
    client_.reset(
        new ChromotingClient(client_context_.get(), this, this, nullptr));
    client_->set_protocol_config(protocol_config_->Clone());
    client_->Start(client_signaling_.get(), std::move(client_authenticator),
                   transport_context, kHostJid, std::string());
  }

  void FetchPin(
      bool pairing_supported,
      const protocol::SecretFetchedCallback& secret_fetched_callback) {
    secret_fetched_callback.Run("123456");
  }

  void MeasureTotalLatency(bool webrtc) {
    scoped_refptr<test::CyclicFrameGenerator> frame_generator =
        test::CyclicFrameGenerator::Create();
    frame_generator->set_draw_barcode(true);

    desktop_environment_factory_.set_frame_generator(base::Bind(
        &test::CyclicFrameGenerator::GenerateFrame, frame_generator));

    StartHostAndClient(webrtc, protocol::ChannelConfig::CODEC_VP8);
    ASSERT_NO_FATAL_FAILURE(WaitConnected());

    base::TimeDelta total_latency_big_frames;
    int big_frame_count = 0;
    base::TimeDelta total_latency_small_frames;
    int small_frame_count = 0;

    int last_frame_id = -1;
    for (int i = 0; i < 30; ++i) {
      scoped_ptr<webrtc::DesktopFrame> frame = ReceiveFrame();
      test::CyclicFrameGenerator::FrameInfo frame_info =
          frame_generator->IdentifyFrame(frame.get());
      base::TimeDelta latency = base::TimeTicks::Now() - frame_info.timestamp;

      if (frame_info.frame_id > last_frame_id) {
        last_frame_id = frame_info.frame_id;

        switch (frame_info.type) {
          case test::CyclicFrameGenerator::FrameType::EMPTY:
            NOTREACHED();
            break;
          case test::CyclicFrameGenerator::FrameType::FULL:
            total_latency_big_frames += latency;
            ++big_frame_count;
            break;
          case test::CyclicFrameGenerator::FrameType::CURSOR:
            total_latency_small_frames += latency;
            ++small_frame_count;
            break;
        }
      } else {
        LOG(ERROR) << "Unexpected duplicate frame " << frame_info.frame_id;
      }
    }

    CHECK(big_frame_count);
    VLOG(0) << "Average latency for big frames: "
            << (total_latency_big_frames / big_frame_count).InMillisecondsF();

    if (small_frame_count) {
      VLOG(0)
          << "Average latency for small frames: "
          << (total_latency_small_frames / small_frame_count).InMillisecondsF();
    }
  }

  base::MessageLoopForIO message_loop_;

  scoped_refptr<FakeNetworkDispatcher> fake_network_dispatcher_;

  base::Thread host_thread_;
  base::Thread capture_thread_;
  base::Thread encode_thread_;
  base::Thread decode_thread_;
  FakeDesktopEnvironmentFactory desktop_environment_factory_;

  FakeCursorShapeStub cursor_shape_stub_;

  scoped_ptr<protocol::CandidateSessionConfig> protocol_config_;

  scoped_ptr<FakeSignalStrategy> host_signaling_;
  scoped_ptr<FakeSignalStrategy> client_signaling_;

  scoped_ptr<ChromotingHost> host_;
  scoped_ptr<ClientContext> client_context_;
  scoped_ptr<ChromotingClient> client_;
  webrtc::DesktopSize frame_size_;
  scoped_ptr<VideoDecoder> video_decoder_;

  scoped_ptr<base::RunLoop> connecting_loop_;
  scoped_ptr<base::RunLoop> waiting_frames_loop_;

  bool client_connected_;
  bool host_connected_;

  base::Closure on_frame_task_;

  scoped_ptr<VideoPacket> last_video_packet_;
  scoped_ptr<webrtc::DesktopFrame> last_video_frame_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProtocolPerfTest);
};

INSTANTIATE_TEST_CASE_P(
    NoDelay,
    ProtocolPerfTest,
    ::testing::Values(NetworkPerformanceParams(0, 0, 0, 0, 0.0)));

INSTANTIATE_TEST_CASE_P(
    HighLatency,
    ProtocolPerfTest,
    ::testing::Values(NetworkPerformanceParams(0, 0, 300, 30, 0.0),
                      NetworkPerformanceParams(0, 0, 30, 10, 0.0)));

INSTANTIATE_TEST_CASE_P(
    OutOfOrder,
    ProtocolPerfTest,
    ::testing::Values(NetworkPerformanceParams(0, 0, 2, 0, 0.01),
                      NetworkPerformanceParams(0, 0, 30, 1, 0.01),
                      NetworkPerformanceParams(0, 0, 30, 1, 0.1),
                      NetworkPerformanceParams(0, 0, 300, 20, 0.01),
                      NetworkPerformanceParams(0, 0, 300, 20, 0.1)));

INSTANTIATE_TEST_CASE_P(
    LimitedBandwidth,
    ProtocolPerfTest,
    ::testing::Values(
        // 100 MBps
        NetworkPerformanceParams(800000000, 800000000, 2, 1, 0.0),
        // 8 MBps
        NetworkPerformanceParams(1000000, 300000, 30, 5, 0.01),
        NetworkPerformanceParams(1000000, 2000000, 30, 5, 0.01),
        // 800 kBps
        NetworkPerformanceParams(100000, 30000, 130, 5, 0.01),
        NetworkPerformanceParams(100000, 200000, 130, 5, 0.01)));

TEST_P(ProtocolPerfTest, StreamFrameRate) {
  StartHostAndClient(false, protocol::ChannelConfig::CODEC_VP8);
  ASSERT_NO_FATAL_FAILURE(WaitConnected());

  base::TimeDelta latency;

  ReceiveFrameAndGetLatency(&latency);
  LOG(INFO) << "First frame latency: " << latency.InMillisecondsF() << "ms";
  ReceiveMultipleFramesAndGetMaxLatency(20, nullptr);

  base::TimeTicks started = base::TimeTicks::Now();
  ReceiveMultipleFramesAndGetMaxLatency(40, &latency);
  base::TimeDelta elapsed = base::TimeTicks::Now() - started;
  LOG(INFO) << "Frame rate: " << (40.0 / elapsed.InSecondsF());
  LOG(INFO) << "Maximum latency: " << latency.InMillisecondsF() << "ms";
}

const int kIntermittentFrameSize = 100 * 1000;

// Frame generator that rewrites the whole screen every 60th frame. Should only
// be used with the VERBATIM codec as the allocated frame may contain arbitrary
// data.
class IntermittentChangeFrameGenerator
    : public base::RefCountedThreadSafe<IntermittentChangeFrameGenerator> {
 public:
  IntermittentChangeFrameGenerator()
      : frame_index_(0) {}

  scoped_ptr<webrtc::DesktopFrame> GenerateFrame(
      webrtc::DesktopCapturer::Callback* callback) {
    const int kWidth = 1000;
    const int kHeight = kIntermittentFrameSize / kWidth / 4;

    bool fresh_frame = false;
    if (frame_index_ % 60 == 0 || !current_frame_) {
      current_frame_.reset(webrtc::SharedDesktopFrame::Wrap(
          new webrtc::BasicDesktopFrame(webrtc::DesktopSize(kWidth, kHeight))));
      fresh_frame = true;
    }
    ++frame_index_;

    scoped_ptr<webrtc::DesktopFrame> result(current_frame_->Share());
    result->mutable_updated_region()->Clear();
    if (fresh_frame) {
      result->mutable_updated_region()->AddRect(
          webrtc::DesktopRect::MakeXYWH(0, 0, kWidth, kHeight));
    }
    return result;
  }

 private:
  ~IntermittentChangeFrameGenerator() {}
  friend class base::RefCountedThreadSafe<IntermittentChangeFrameGenerator>;

  int frame_index_;
  scoped_ptr<webrtc::SharedDesktopFrame> current_frame_;

  DISALLOW_COPY_AND_ASSIGN(IntermittentChangeFrameGenerator);
};

TEST_P(ProtocolPerfTest, IntermittentChanges) {
  desktop_environment_factory_.set_frame_generator(
      base::Bind(&IntermittentChangeFrameGenerator::GenerateFrame,
                 new IntermittentChangeFrameGenerator()));

  StartHostAndClient(false, protocol::ChannelConfig::CODEC_VERBATIM);
  ASSERT_NO_FATAL_FAILURE(WaitConnected());

  ReceiveFrameAndGetLatency(nullptr);

  base::TimeDelta expected = GetParam().latency_average;
  if (GetParam().bandwidth > 0) {
    expected += base::TimeDelta::FromSecondsD(kIntermittentFrameSize /
                                              GetParam().bandwidth);
  }
  LOG(INFO) << "Expected: " << expected.InMillisecondsF() << "ms";

  base::TimeDelta sum;

  const int kFrames = 5;
  for (int i = 0; i < kFrames; ++i) {
    base::TimeDelta latency;
    ReceiveFrameAndGetLatency(&latency);
    LOG(INFO) << "Latency: " << latency.InMillisecondsF()
              << "ms Encode: " << last_video_packet_->encode_time_ms()
              << "ms Capture: " << last_video_packet_->capture_time_ms()
              << "ms";
    sum += latency;
  }

  LOG(INFO) << "Average: " << (sum / kFrames).InMillisecondsF();
}

TEST_P(ProtocolPerfTest, TotalLatencyIce) {
  MeasureTotalLatency(false);
}

TEST_P(ProtocolPerfTest, TotalLatencyWebrtc) {
  MeasureTotalLatency(true);
}

}  // namespace remoting
