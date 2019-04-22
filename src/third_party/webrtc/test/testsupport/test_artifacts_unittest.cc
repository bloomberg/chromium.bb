/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/test_artifacts.h"

#include <string.h>
#include <string>

#include "rtc_base/flags.h"
#include "rtc_base/system/file_wrapper.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

WEBRTC_DECLARE_string(test_artifacts_dir);

namespace webrtc {
namespace test {

TEST(IsolatedOutputTest, ShouldRejectInvalidIsolatedOutDir) {
  const char* backup = FLAG_test_artifacts_dir;
  FLAG_test_artifacts_dir = "";
  ASSERT_FALSE(WriteToTestArtifactsDir("a-file", "some-contents"));
  FLAG_test_artifacts_dir = backup;
}

TEST(IsolatedOutputTest, ShouldRejectInvalidFileName) {
  ASSERT_FALSE(WriteToTestArtifactsDir(nullptr, "some-contents"));
  ASSERT_FALSE(WriteToTestArtifactsDir("", "some-contents"));
}

// Sets isolated_out_dir=<a-writable-path> to execute this test.
TEST(IsolatedOutputTest, ShouldBeAbleToWriteContent) {
  const char* filename = "a-file";
  const char* content = "some-contents";
  if (WriteToTestArtifactsDir(filename, content)) {
    std::string out_file = JoinFilename(FLAG_test_artifacts_dir, filename);
    FileWrapper input = FileWrapper::OpenReadOnly(out_file);
    EXPECT_TRUE(input.is_open());
    EXPECT_TRUE(input.Rewind());
    uint8_t buffer[32];
    EXPECT_EQ(input.Read(buffer, strlen(content)), strlen(content));
    buffer[strlen(content)] = 0;
    EXPECT_EQ(std::string(content),
              std::string(reinterpret_cast<char*>(buffer)));
    input.Close();

    EXPECT_TRUE(RemoveFile(out_file));
  }
}

}  // namespace test
}  // namespace webrtc
