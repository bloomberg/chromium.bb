/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/api/create_peerconnection_quality_test_fixture.h"

#include <utility>

#include "absl/memory/memory.h"
#include "test/pc/e2e/peer_connection_quality_test.h"

namespace webrtc {
namespace test {

std::unique_ptr<PeerConnectionE2EQualityTestFixture>
CreatePeerConnectionE2EQualityTestFixture(
    std::string test_case_name,
    std::unique_ptr<AudioQualityAnalyzerInterface> audio_quality_analyzer,
    std::unique_ptr<VideoQualityAnalyzerInterface> video_quality_analyzer) {
  return absl::make_unique<webrtc::test::PeerConnectionE2EQualityTest>(
      std::move(test_case_name), std::move(audio_quality_analyzer),
      std::move(video_quality_analyzer));
}

}  // namespace test
}  // namespace webrtc
