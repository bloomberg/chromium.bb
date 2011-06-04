// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_video_decoder.h"

#include "ppapi/c/dev/ppb_video_decoder_dev.h"
#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(VideoDecoder);

bool TestVideoDecoder::Init() {
  video_decoder_interface_ = reinterpret_cast<PPB_VideoDecoder_Dev const*>(
      pp::Module::Get()->GetBrowserInterface(PPB_VIDEODECODER_DEV_INTERFACE));
  var_interface_ = reinterpret_cast<PPB_Var const*>(
      pp::Module::Get()->GetBrowserInterface(PPB_VAR_INTERFACE));
  return video_decoder_interface_ && var_interface_ && InitTestingInterface();
}

void TestVideoDecoder::RunTest() {
  RUN_TEST(CreateAndInitialize);
}

void TestVideoDecoder::QuitMessageLoop() {
  testing_interface_->QuitMessageLoop(instance_->pp_instance());
}

std::string TestVideoDecoder::TestCreateAndInitialize() {
  PP_Resource decoder = video_decoder_interface_->Create(
      instance_->pp_instance());
  if (decoder == 0)
    return "Create: error creating the decoder";

  int32_t pp_error = video_decoder_interface_->Initialize(
      decoder, NULL, PP_BlockUntilComplete());
  pp::Module::Get()->core()->ReleaseResource(decoder);
  if (pp_error != PP_ERROR_BADARGUMENT)
    return "Initialize: error detecting null callback";

  PASS();
}
