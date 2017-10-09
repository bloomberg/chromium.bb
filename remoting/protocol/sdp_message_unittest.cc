// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/sdp_message.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace protocol {

// Verify that SDP is normalized by removing empty lines and normalizing
// line-endings to \n.
TEST(SdpMessages, Normalize) {
  SdpMessage sdp_message("a=foo\n\r\nb=bar");
  EXPECT_EQ("a=foo\nb=bar\n", sdp_message.ToString());
}

TEST(SdpMessages, AddCodecParameter) {
  std::string kSourceSdp =
      "a=group:BUNDLE video\n"
      "m=video 9 UDP/TLS/RTP/SAVPF 96\n"
      "a=rtpmap:96 VP8/90000\n"
      "a=rtcp-fb:96 transport-cc\n";
  SdpMessage sdp_message(kSourceSdp);
  EXPECT_TRUE(sdp_message.AddCodecParameter("VP8", "test_param=1"));
  EXPECT_EQ(
      "a=group:BUNDLE video\n"
      "m=video 9 UDP/TLS/RTP/SAVPF 96\n"
      "a=rtpmap:96 VP8/90000\n"
      "a=fmtp:96 test_param=1\n"
      "a=rtcp-fb:96 transport-cc\n",
      sdp_message.ToString());
}

TEST(SdpMessages, AddCodecParameterMissingCodec) {
  std::string kSourceSdp =
      "a=group:BUNDLE audio video\n"
      "m=audio 9 UDP/TLS/RTP/SAVPF 111\n"
      "a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level\n"
      "a=rtpmap:111 opus/48000/2\n"
      "a=rtcp-fb:111 transport-cc\n"
      "m=video 9 UDP/TLS/RTP/SAVPF 96\n"
      "a=rtpmap:96 VP9/90000\n"
      "a=rtcp-fb:96 transport-cc\n"
      "m=application 9 DTLS/SCTP 5000\n";
  SdpMessage sdp_message(kSourceSdp);
  EXPECT_FALSE(sdp_message.AddCodecParameter("VP8", "test_param=1"));
  EXPECT_EQ(kSourceSdp, sdp_message.ToString());
}

TEST(SdpMessages, PreferVideoCodec_SdpIsUnchanged) {
  static const std::string kSourceSdp =
      "m=video 98 UDP/TLS/RTP/SAVPF 96 97 98 99 100\n"
      "a=rtpmap:96 VP8/90000\n"
      "a=fmtp:96\n"
      "a=rtpmap:97 VP8.1/90000\n"
      "a=fmtp:97\n"
      "a=rtpmap:98 VP9/90000\n"
      "a=fmtp:98\n"
      "a=rtpmap:99 VP9.1/90000\n"
      "a=fmtp:99\n"
      "a=rtpmap:100 H264/90000\n"
      "a=fmtp:100\n";
  SdpMessage sdp_message(kSourceSdp);
  EXPECT_TRUE(sdp_message.PreferVideoCodec("VP8"));
  EXPECT_EQ(kSourceSdp, sdp_message.ToString());
}

TEST(SdpMessages, PreferVideoCodec_MoveToTheStartOfTheList) {
  static const std::string kSourceSdp =
      "m=video 98 UDP/TLS/RTP/SAVPF 96 97 98 99 100\n"
      "a=rtpmap:96 VP8/90000\n"
      "a=fmtp:96\n"
      "a=rtpmap:97 VP8.1/90000\n"
      "a=fmtp:97\n"
      "a=rtpmap:98 VP9/90000\n"
      "a=fmtp:98\n"
      "a=rtpmap:99 VP9.1/90000\n"
      "a=fmtp:99\n"
      "a=rtpmap:100 H264/90000\n"
      "a=fmtp:100\n";
  SdpMessage sdp_message(kSourceSdp);
  EXPECT_TRUE(sdp_message.PreferVideoCodec("VP8.1"));
  EXPECT_EQ("m=video 98 UDP/TLS/RTP/SAVPF 97 96 98 99 100\n"
            "a=rtpmap:96 VP8/90000\n"
            "a=fmtp:96\n"
            "a=rtpmap:97 VP8.1/90000\n"
            "a=fmtp:97\n"
            "a=rtpmap:98 VP9/90000\n"
            "a=fmtp:98\n"
            "a=rtpmap:99 VP9.1/90000\n"
            "a=fmtp:99\n"
            "a=rtpmap:100 H264/90000\n"
            "a=fmtp:100\n",
            sdp_message.ToString());

  sdp_message = SdpMessage(kSourceSdp);
  EXPECT_TRUE(sdp_message.PreferVideoCodec("VP9"));
  EXPECT_EQ("m=video 98 UDP/TLS/RTP/SAVPF 98 96 97 99 100\n"
            "a=rtpmap:96 VP8/90000\n"
            "a=fmtp:96\n"
            "a=rtpmap:97 VP8.1/90000\n"
            "a=fmtp:97\n"
            "a=rtpmap:98 VP9/90000\n"
            "a=fmtp:98\n"
            "a=rtpmap:99 VP9.1/90000\n"
            "a=fmtp:99\n"
            "a=rtpmap:100 H264/90000\n"
            "a=fmtp:100\n",
            sdp_message.ToString());

  sdp_message = SdpMessage(kSourceSdp);
  EXPECT_TRUE(sdp_message.PreferVideoCodec("VP9.1"));
  EXPECT_EQ("m=video 98 UDP/TLS/RTP/SAVPF 99 96 97 98 100\n"
            "a=rtpmap:96 VP8/90000\n"
            "a=fmtp:96\n"
            "a=rtpmap:97 VP8.1/90000\n"
            "a=fmtp:97\n"
            "a=rtpmap:98 VP9/90000\n"
            "a=fmtp:98\n"
            "a=rtpmap:99 VP9.1/90000\n"
            "a=fmtp:99\n"
            "a=rtpmap:100 H264/90000\n"
            "a=fmtp:100\n",
            sdp_message.ToString());

  sdp_message = SdpMessage(kSourceSdp);
  EXPECT_TRUE(sdp_message.PreferVideoCodec("H264"));
  EXPECT_EQ("m=video 98 UDP/TLS/RTP/SAVPF 100 96 97 98 99\n"
            "a=rtpmap:96 VP8/90000\n"
            "a=fmtp:96\n"
            "a=rtpmap:97 VP8.1/90000\n"
            "a=fmtp:97\n"
            "a=rtpmap:98 VP9/90000\n"
            "a=fmtp:98\n"
            "a=rtpmap:99 VP9.1/90000\n"
            "a=fmtp:99\n"
            "a=rtpmap:100 H264/90000\n"
            "a=fmtp:100\n",
            sdp_message.ToString());
}

TEST(SdpMessages, PreferVideoCodec_UnknownCodec) {
  static const std::string kSourceSdp =
      "m=video 98 UDP/TLS/RTP/SAVPF 96 97 98 99 100\n"
      "a=rtpmap:96 VP8/90000\n"
      "a=fmtp:96\n"
      "a=rtpmap:97 VP8.1/90000\n"
      "a=fmtp:97\n"
      "a=rtpmap:98 VP9/90000\n"
      "a=fmtp:98\n"
      "a=rtpmap:99 VP9.1/90000\n"
      "a=fmtp:99\n"
      "a=rtpmap:100 H264/90000\n"
      "a=fmtp:100\n";
  SdpMessage sdp_message(kSourceSdp);
  EXPECT_FALSE(sdp_message.PreferVideoCodec("VP7"));
  EXPECT_EQ("m=video 98 UDP/TLS/RTP/SAVPF 96 97 98 99 100\n"
            "a=rtpmap:96 VP8/90000\n"
            "a=fmtp:96\n"
            "a=rtpmap:97 VP8.1/90000\n"
            "a=fmtp:97\n"
            "a=rtpmap:98 VP9/90000\n"
            "a=fmtp:98\n"
            "a=rtpmap:99 VP9.1/90000\n"
            "a=fmtp:99\n"
            "a=rtpmap:100 H264/90000\n"
            "a=fmtp:100\n",
            sdp_message.ToString());
}

TEST(SdpMessages, PreferVideoCodec_MultiplePlayloads) {
  static const std::string kSourceSdp =
      "m=video 98 UDP/TLS/RTP/SAVPF 96 97 100 101 102 98 99\n"
      "a=rtpmap:96 VP8/90000\n"
      "a=fmtp:96\n"
      "a=rtpmap:97 VP8.1/90000\n"
      "a=fmtp:97\n"
      "a=rtpmap:101 VP9/90000\n"
      "a=fmtp:101\n"
      "a=rtpmap:98 VP9/90000\n"
      "a=fmtp:98\n"
      "a=rtpmap:99 VP9.1/90000\n"
      "a=fmtp:99\n"
      "a=rtpmap:102 VP9/90000\n"
      "a=fmtp:102\n"
      "a=rtpmap:103 VP9/90000\n"
      "a=fmtp:103\n"
      "a=rtpmap:100 H264/90000\n"
      "a=fmtp:100\n";
  SdpMessage sdp_message(kSourceSdp);
  EXPECT_TRUE(sdp_message.PreferVideoCodec("VP9"));
  EXPECT_EQ("m=video 98 UDP/TLS/RTP/SAVPF 102 98 101 96 97 100 99\n"
            "a=rtpmap:96 VP8/90000\n"
            "a=fmtp:96\n"
            "a=rtpmap:97 VP8.1/90000\n"
            "a=fmtp:97\n"
            "a=rtpmap:101 VP9/90000\n"
            "a=fmtp:101\n"
            "a=rtpmap:98 VP9/90000\n"
            "a=fmtp:98\n"
            "a=rtpmap:99 VP9.1/90000\n"
            "a=fmtp:99\n"
            "a=rtpmap:102 VP9/90000\n"
            "a=fmtp:102\n"
            "a=rtpmap:103 VP9/90000\n"
            "a=fmtp:103\n"
            "a=rtpmap:100 H264/90000\n"
            "a=fmtp:100\n",
            sdp_message.ToString());
}

}  // namespace protocol
}  // namespace remoting
