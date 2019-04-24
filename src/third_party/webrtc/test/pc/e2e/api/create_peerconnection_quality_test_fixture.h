/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_PC_E2E_API_CREATE_PEERCONNECTION_QUALITY_TEST_FIXTURE_H_
#define TEST_PC_E2E_API_CREATE_PEERCONNECTION_QUALITY_TEST_FIXTURE_H_

#include <memory>

#include "test/pc/e2e/api/audio_quality_analyzer_interface.h"
#include "test/pc/e2e/api/peerconnection_quality_test_fixture.h"
#include "test/pc/e2e/api/video_quality_analyzer_interface.h"

namespace webrtc {
namespace test {

// API is in development. Can be changed/removed without notice.
// Create test fixture to establish test call between Alice and Bob.
// During the test Alice will be caller and Bob will answer the call.
// |test_case_name| is a name of test case, that will be used for all metrics
// reporting.
std::unique_ptr<PeerConnectionE2EQualityTestFixture>
CreatePeerConnectionE2EQualityTestFixture(
    std::string test_case_name,
    std::unique_ptr<AudioQualityAnalyzerInterface> audio_quality_analyzer,
    std::unique_ptr<VideoQualityAnalyzerInterface> video_quality_analyzer);

}  // namespace test
}  // namespace webrtc

#endif  // TEST_PC_E2E_API_CREATE_PEERCONNECTION_QUALITY_TEST_FIXTURE_H_
