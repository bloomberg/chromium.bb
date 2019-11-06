/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_utility.h"
#include "pc/media_session.h"
#include "pc/session_description.h"
#include "test/field_trial.h"
#include "test/gtest.h"
#include "test/peer_scenario/peer_scenario.h"

namespace webrtc {
namespace test {
namespace {
RtpHeaderExtensionMap AudioExtensions(
    const SessionDescriptionInterface& session) {
  auto* audio_desc =
      cricket::GetFirstAudioContentDescription(session.description());
  return RtpHeaderExtensionMap(audio_desc->rtp_header_extensions());
}

absl::optional<RTPHeaderExtension> GetRtpPacketExtensions(
    const rtc::ArrayView<const uint8_t> packet,
    const RtpHeaderExtensionMap& extension_map) {
  RtpUtility::RtpHeaderParser rtp_parser(packet.data(), packet.size());
  if (!rtp_parser.RTCP()) {
    RTPHeader header;
    if (rtp_parser.Parse(&header, &extension_map, true)) {
      return header.extension;
    }
  }
  return absl::nullopt;
}

}  // namespace

TEST(RemoteEstimateEndToEnd, OfferedCapabilityIsInAnswer) {
  PeerScenario s;

  auto* caller = s.CreateClient(PeerScenarioClient::Config());
  auto* callee = s.CreateClient(PeerScenarioClient::Config());

  auto send_link = {s.net()->NodeBuilder().Build().node};
  auto ret_link = {s.net()->NodeBuilder().Build().node};

  s.net()->CreateRoute(caller->endpoint(), send_link, callee->endpoint());
  s.net()->CreateRoute(callee->endpoint(), ret_link, caller->endpoint());

  auto signaling = s.ConnectSignaling(caller, callee, send_link, ret_link);
  caller->CreateVideo("VIDEO", PeerScenarioClient::VideoSendTrackConfig());
  rtc::Event offer_exchange_done;
  signaling.NegotiateSdp(
      [](SessionDescriptionInterface* offer) {
        for (auto& cont : offer->description()->contents()) {
          cont.media_description()->set_remote_estimate(true);
        }
      },
      [&](const SessionDescriptionInterface& answer) {
        for (auto& cont : answer.description()->contents()) {
          EXPECT_TRUE(cont.media_description()->remote_estimate());
        }
        offer_exchange_done.Set();
      });
  EXPECT_TRUE(s.WaitAndProcess(&offer_exchange_done));
}

TEST(RemoteEstimateEndToEnd, AudioUsesAbsSendTimeExtension) {
  ScopedFieldTrials trials("WebRTC-KeepAbsSendTimeExtension/Enabled/");
  // Defined before PeerScenario so it gets destructed after, to avoid use after free.
  rtc::Event received_abs_send_time;
  PeerScenario s;

  auto* caller = s.CreateClient(PeerScenarioClient::Config());
  auto* callee = s.CreateClient(PeerScenarioClient::Config());

  auto send_node = s.net()->NodeBuilder().Build().node;
  auto ret_node = s.net()->NodeBuilder().Build().node;

  s.net()->CreateRoute(caller->endpoint(), {send_node}, callee->endpoint());
  s.net()->CreateRoute(callee->endpoint(), {ret_node}, caller->endpoint());

  auto signaling = s.ConnectSignaling(caller, callee, {send_node}, {ret_node});
  caller->CreateAudio("AUDIO", cricket::AudioOptions());
  signaling.StartIceSignaling();
  RtpHeaderExtensionMap extension_map;
  rtc::Event offer_exchange_done;
  signaling.NegotiateSdp(
      [&extension_map](SessionDescriptionInterface* offer) {
        extension_map = AudioExtensions(*offer);
        EXPECT_TRUE(extension_map.IsRegistered(kRtpExtensionAbsoluteSendTime));
      },
      [&](const SessionDescriptionInterface& answer) {
        EXPECT_TRUE(AudioExtensions(answer).IsRegistered(
            kRtpExtensionAbsoluteSendTime));
        offer_exchange_done.Set();
      });
  EXPECT_TRUE(s.WaitAndProcess(&offer_exchange_done));
  send_node->router()->SetWatcher(
      [extension_map, &received_abs_send_time](const EmulatedIpPacket& packet) {
        auto extensions = GetRtpPacketExtensions(packet.data, extension_map);
        if (extensions) {
          EXPECT_TRUE(extensions->hasAbsoluteSendTime);
          received_abs_send_time.Set();
        }
      });
  EXPECT_TRUE(s.WaitAndProcess(&received_abs_send_time));
}
}  // namespace test
}  // namespace webrtc
