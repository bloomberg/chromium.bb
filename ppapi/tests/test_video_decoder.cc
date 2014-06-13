// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_video_decoder.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/video_decoder.h"
#include "ppapi/lib/gl/gles2/gl2ext_ppapi.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(VideoDecoder);

static const bool kAllowSoftwareFallback = true;

bool TestVideoDecoder::Init() {
  video_decoder_interface_ = static_cast<const PPB_VideoDecoder_0_1*>(
      pp::Module::Get()->GetBrowserInterface(PPB_VIDEODECODER_INTERFACE_0_1));
  const int width = 16;
  const int height = 16;
  const int32_t attribs[] = {PP_GRAPHICS3DATTRIB_WIDTH, width,
                             PP_GRAPHICS3DATTRIB_HEIGHT, height,
                             PP_GRAPHICS3DATTRIB_NONE};
  graphics_3d_ = pp::Graphics3D(instance_, attribs);
  return video_decoder_interface_ && CheckTestingInterface();
}

void TestVideoDecoder::RunTests(const std::string& filter) {
  RUN_CALLBACK_TEST(TestVideoDecoder, Create, filter);
}

std::string TestVideoDecoder::TestCreate() {
  // Test that Initialize fails with a bad Graphics3D resource.
  {
    pp::VideoDecoder video_decoder(instance_);
    ASSERT_FALSE(video_decoder.is_null());

    TestCompletionCallback callback(instance_->pp_instance(), callback_type());
    pp::Graphics3D null_graphics_3d;
    callback.WaitForResult(video_decoder.Initialize(null_graphics_3d,
                                                    PP_VIDEOPROFILE_VP8MAIN,
                                                    kAllowSoftwareFallback,
                                                    callback.GetCallback()));
    ASSERT_EQ(PP_ERROR_BADRESOURCE, callback.result());
  }
  // Test that Initialize fails with a bad profile enum value.
  {
    pp::VideoDecoder video_decoder(instance_);
    TestCompletionCallback callback(instance_->pp_instance(), callback_type());
    const PP_VideoProfile kInvalidProfile = static_cast<PP_VideoProfile>(-1);
    callback.WaitForResult(video_decoder.Initialize(graphics_3d_,
                                                    kInvalidProfile,
                                                    kAllowSoftwareFallback,
                                                    callback.GetCallback()));
    ASSERT_EQ(PP_ERROR_BADARGUMENT, callback.result());
  }
  // Test that Initialize succeeds if we can create a Graphics3D resources and
  // if we allow software fallback to VP8, which should always be supported.
  if (!graphics_3d_.is_null()) {
    pp::VideoDecoder video_decoder(instance_);
    TestCompletionCallback callback(instance_->pp_instance(), callback_type());
    callback.WaitForResult(video_decoder.Initialize(graphics_3d_,
                                                    PP_VIDEOPROFILE_VP8MAIN,
                                                    kAllowSoftwareFallback,
                                                    callback.GetCallback()));
    ASSERT_EQ(PP_OK, callback.result());
  }

  PASS();
}
