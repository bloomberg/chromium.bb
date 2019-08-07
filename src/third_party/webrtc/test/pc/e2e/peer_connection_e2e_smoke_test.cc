/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdint>
#include <memory>

#include "absl/memory/memory.h"
#include "api/test/create_network_emulation_manager.h"
#include "api/test/create_peerconnection_quality_test_fixture.h"
#include "api/test/network_emulation_manager.h"
#include "api/test/peerconnection_quality_test_fixture.h"
#include "call/simulated_network.h"
#include "test/gtest.h"
#include "test/pc/e2e/analyzer/audio/default_audio_quality_analyzer.h"
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer.h"
#include "test/pc/e2e/network_quality_metrics_reporter.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {
namespace webrtc_pc_e2e {

// IOS debug builds can be quite slow, disabling to avoid issues with timeouts.
#if defined(WEBRTC_IOS) && defined(WEBRTC_ARCH_ARM64) && !defined(NDEBUG)
#define MAYBE_RunWithEmulatedNetwork DISABLED_RunWithEmulatedNetwork
#else
#define MAYBE_RunWithEmulatedNetwork RunWithEmulatedNetwork
#endif
TEST(PeerConnectionE2EQualityTestSmokeTest, MAYBE_RunWithEmulatedNetwork) {
  using PeerConfigurer = PeerConnectionE2EQualityTestFixture::PeerConfigurer;
  using RunParams = PeerConnectionE2EQualityTestFixture::RunParams;
  using VideoConfig = PeerConnectionE2EQualityTestFixture::VideoConfig;
  using AudioConfig = PeerConnectionE2EQualityTestFixture::AudioConfig;
  using ScreenShareConfig =
      PeerConnectionE2EQualityTestFixture::ScreenShareConfig;
  using ScrollingParams = PeerConnectionE2EQualityTestFixture::ScrollingParams;

  // Setup emulated network
  std::unique_ptr<NetworkEmulationManager> network_emulation_manager =
      CreateNetworkEmulationManager();

  auto alice_network_behavior =
      absl::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig());
  SimulatedNetwork* alice_network_behavior_ptr = alice_network_behavior.get();
  EmulatedNetworkNode* alice_node =
      network_emulation_manager->CreateEmulatedNode(
          std::move(alice_network_behavior));
  EmulatedNetworkNode* bob_node = network_emulation_manager->CreateEmulatedNode(
      absl::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig()));
  EmulatedEndpoint* alice_endpoint =
      network_emulation_manager->CreateEndpoint(EmulatedEndpointConfig());
  EmulatedEndpoint* bob_endpoint =
      network_emulation_manager->CreateEndpoint(EmulatedEndpointConfig());
  network_emulation_manager->CreateRoute(alice_endpoint, {alice_node},
                                         bob_endpoint);
  network_emulation_manager->CreateRoute(bob_endpoint, {bob_node},
                                         alice_endpoint);

  // Create analyzers.
  std::unique_ptr<VideoQualityAnalyzerInterface> video_quality_analyzer =
      absl::make_unique<DefaultVideoQualityAnalyzer>();
  // This is only done for the sake of smoke testing. In general there should
  // be no need to explicitly pull data from analyzers after the run.
  auto* video_analyzer_ptr =
      static_cast<DefaultVideoQualityAnalyzer*>(video_quality_analyzer.get());

  std::unique_ptr<AudioQualityAnalyzerInterface> audio_quality_analyzer =
      absl::make_unique<DefaultAudioQualityAnalyzer>();

  auto fixture = CreatePeerConnectionE2EQualityTestFixture(
      "smoke_test", std::move(audio_quality_analyzer),
      std::move(video_quality_analyzer));
  fixture->ExecuteAt(TimeDelta::seconds(2),
                     [alice_network_behavior_ptr](TimeDelta) {
                       BuiltInNetworkBehaviorConfig config;
                       config.loss_percent = 5;
                       alice_network_behavior_ptr->SetConfig(config);
                     });

  // Setup components. We need to provide rtc::NetworkManager compatible with
  // emulated network layer.
  EmulatedNetworkManagerInterface* alice_network =
      network_emulation_manager->CreateEmulatedNetworkManagerInterface(
          {alice_endpoint});
  fixture->AddPeer(alice_network->network_thread(),
                   alice_network->network_manager(), [](PeerConfigurer* alice) {
                     VideoConfig video(640, 360, 30);
                     video.stream_label = "alice-video";
                     alice->AddVideoConfig(std::move(video));

                     AudioConfig audio;
                     audio.stream_label = "alice-audio";
                     audio.mode = AudioConfig::Mode::kFile;
                     audio.input_file_name = test::ResourcePath(
                         "pc_quality_smoke_test_alice_source", "wav");
                     alice->SetAudioConfig(std::move(audio));
                   });

  EmulatedNetworkManagerInterface* bob_network =
      network_emulation_manager->CreateEmulatedNetworkManagerInterface(
          {bob_endpoint});
  fixture->AddPeer(
      bob_network->network_thread(), bob_network->network_manager(),
      [](PeerConfigurer* bob) {
        VideoConfig video(640, 360, 30);
        video.stream_label = "bob-video";
        bob->AddVideoConfig(std::move(video));

        VideoConfig screenshare(640, 360, 30);
        screenshare.stream_label = "bob-screenshare";
        screenshare.screen_share_config =
            ScreenShareConfig(TimeDelta::seconds(2));
        screenshare.screen_share_config->scrolling_params = ScrollingParams(
            TimeDelta::ms(1800), kDefaultSlidesWidth, kDefaultSlidesHeight);
        bob->AddVideoConfig(screenshare);

        AudioConfig audio;
        audio.stream_label = "bob-audio";
        audio.mode = AudioConfig::Mode::kFile;
        audio.input_file_name =
            test::ResourcePath("pc_quality_smoke_test_bob_source", "wav");
        bob->SetAudioConfig(std::move(audio));
      });

  fixture->AddQualityMetricsReporter(
      absl::make_unique<NetworkQualityMetricsReporter>(alice_network,
                                                       bob_network));

  RunParams run_params(TimeDelta::seconds(7));
  run_params.video_codec_name = cricket::kVp9CodecName;
  run_params.video_codec_required_params = {{"profile-id", "0"}};
  run_params.use_flex_fec = true;
  run_params.use_ulp_fec = true;
  run_params.video_encoder_bitrate_multiplier = 1.1;
  fixture->Run(run_params);

  EXPECT_GE(fixture->GetRealTestDuration(), run_params.run_duration);
  for (auto stream_label : video_analyzer_ptr->GetKnownVideoStreams()) {
    FrameCounters stream_conters =
        video_analyzer_ptr->GetPerStreamCounters().at(stream_label);
    // 150 = 30fps * 5s. On some devices pipeline can be too slow, so it can
    // happen, that frames will stuck in the middle, so we actually can't force
    // real constraints here, so lets just check, that at least 1 frame passed
    // whole pipeline.
    EXPECT_GE(stream_conters.captured, 150);
    EXPECT_GE(stream_conters.pre_encoded, 1);
    EXPECT_GE(stream_conters.encoded, 1);
    EXPECT_GE(stream_conters.received, 1);
    EXPECT_GE(stream_conters.decoded, 1);
    EXPECT_GE(stream_conters.rendered, 1);
  }
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
