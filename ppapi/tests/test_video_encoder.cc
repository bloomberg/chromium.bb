// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_video_encoder.h"

#include "ppapi/c/pp_codecs.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/video_encoder.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(VideoEncoder);

bool TestVideoEncoder::Init() {
  video_encoder_interface_ = static_cast<const PPB_VideoEncoder_0_1*>(
      pp::Module::Get()->GetBrowserInterface(PPB_VIDEOENCODER_INTERFACE_0_1));
  return video_encoder_interface_ && CheckTestingInterface();
}

void TestVideoEncoder::RunTests(const std::string& filter) {
  RUN_CALLBACK_TEST(TestVideoEncoder, Create, filter);
}

std::string TestVideoEncoder::TestCreate() {
  // Test that we get results for supported formats.
  {
    pp::VideoEncoder video_encoder(instance_);
    ASSERT_FALSE(video_encoder.is_null());

    TestCompletionCallbackWithOutput<std::vector<PP_VideoProfileDescription> >
        callback(instance_->pp_instance(), false);
    callback.WaitForResult(
        video_encoder.GetSupportedProfiles(callback.GetCallback()));

    ASSERT_GE(callback.output().size(), 0U);
  }
  // TODO(llandwerlin): add more tests once software video encode is
  // available.

  PASS();
}
