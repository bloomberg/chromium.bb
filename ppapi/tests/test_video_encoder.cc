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
  // Test that we get results for supported formats. We should at
  // least get the software supported formats.
  {
    pp::VideoEncoder video_encoder(instance_);
    ASSERT_FALSE(video_encoder.is_null());

    TestCompletionCallbackWithOutput<std::vector<PP_VideoProfileDescription> >
        callback(instance_->pp_instance(), false);
    callback.WaitForResult(
        video_encoder.GetSupportedProfiles(callback.GetCallback()));

    ASSERT_GE(callback.result(), 1U);

    const std::vector<PP_VideoProfileDescription> video_profiles =
        callback.output();
    ASSERT_GE(video_profiles.size(), 1U);

    bool found_vp8 = false;
    for (uint32_t i = 0; i < video_profiles.size(); ++i) {
      const PP_VideoProfileDescription& description = video_profiles[i];
      if (description.profile == PP_VIDEOPROFILE_VP8_ANY &&
          description.hardware_accelerated == PP_FALSE) {
        ASSERT_GE(description.max_framerate_numerator /
                      description.max_framerate_denominator,
                  30U);
        found_vp8 = true;
      }
    }
    ASSERT_TRUE(found_vp8);
  }
  // Test that initializing the encoder with incorrect size fails.
  {
    pp::VideoEncoder video_encoder(instance_);
    ASSERT_FALSE(video_encoder.is_null());
    pp::Size video_size(0, 0);

    TestCompletionCallback callback(instance_->pp_instance(), false);
    callback.WaitForResult(
        video_encoder.Initialize(PP_VIDEOFRAME_FORMAT_I420,
                                 video_size,
                                 PP_VIDEOPROFILE_VP8_ANY,
                                 1000000,
                                 PP_HARDWAREACCELERATION_WITHFALLBACK,
                                 callback.GetCallback()));

    ASSERT_EQ(PP_ERROR_BADARGUMENT, callback.result());
  }
  // Test that initializing the encoder with software VP8 succeeds.
  {
    pp::VideoEncoder video_encoder(instance_);
    ASSERT_FALSE(video_encoder.is_null());
    pp::Size video_size(640, 480);

    TestCompletionCallback callback(instance_->pp_instance(), false);
    callback.WaitForResult(
        video_encoder.Initialize(PP_VIDEOFRAME_FORMAT_I420,
                                 video_size,
                                 PP_VIDEOPROFILE_VP8_ANY,
                                 1000000,
                                 PP_HARDWAREACCELERATION_WITHFALLBACK,
                                 callback.GetCallback()));

    ASSERT_EQ(PP_OK, callback.result());

    pp::Size coded_size;
    ASSERT_EQ(PP_OK, video_encoder.GetFrameCodedSize(&coded_size));
    ASSERT_GE(coded_size.GetArea(), video_size.GetArea());
    ASSERT_GE(video_encoder.GetFramesRequired(), 1);

    TestCompletionCallbackWithOutput<pp::VideoFrame> get_video_frame(
        instance_->pp_instance(), false);
    get_video_frame.WaitForResult(
        video_encoder.GetVideoFrame(get_video_frame.GetCallback()));
    ASSERT_EQ(PP_OK, get_video_frame.result());

    pp::Size video_frame_size;
    ASSERT_TRUE(get_video_frame.output().GetSize(&video_frame_size));
    ASSERT_EQ(coded_size.GetArea(), video_frame_size.GetArea());
  }

  PASS();
}
