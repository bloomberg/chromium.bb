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

}  // namespace protocol
}  // namespace remoting
