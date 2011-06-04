// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_video_decoder.h"

#include "ppapi/c/dev/ppb_video_decoder_dev.h"
#include "ppapi/c/dev/ppb_testing_dev.h"
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
  instance_->LogTest("Create", TestCreate());
}

void TestVideoDecoder::QuitMessageLoop() {
  testing_interface_->QuitMessageLoop(instance_->pp_instance());
}

std::string TestVideoDecoder::TestCreate() {
  PP_Resource decoder = video_decoder_interface_->Create(
      instance_->pp_instance(), NULL, PP_MakeCompletionCallback(NULL, NULL));
  if (decoder == 0) {
    return "Error creating the decoder";
  }
  PASS();
}
