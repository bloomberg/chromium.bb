// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From private/ppb_image_capture_private.idl,
//   modified Wed Aug 13 17:26:13 2014.

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_image_capture_private.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"
#include "ppapi/thunk/ppb_image_capture_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance,
                   struct PP_Var camera_source_id,
                   PPB_ImageCapture_Private_ErrorCallback error_callback,
                   void* user_data) {
  VLOG(4) << "PPB_ImageCapture_Private::Create()";
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateImageCapturePrivate(instance,
                                                      camera_source_id,
                                                      error_callback,
                                                      user_data);
}

PP_Bool IsImageCapture(PP_Resource resource) {
  VLOG(4) << "PPB_ImageCapture_Private::IsImageCapture()";
  EnterResource<PPB_ImageCapture_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t Close(PP_Resource resource, struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_ImageCapture_Private::Close()";
  EnterResource<PPB_ImageCapture_API> enter(resource, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Close(enter.callback()));
}

int32_t SetConfig(PP_Resource image_capture,
                  PP_Resource config,
                  struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_ImageCapture_Private::SetConfig()";
  EnterResource<PPB_ImageCapture_API> enter(image_capture, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->SetConfig(config, enter.callback()));
}

int32_t GetConfig(PP_Resource image_capture,
                  PP_Resource* config,
                  struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_ImageCapture_Private::GetConfig()";
  EnterResource<PPB_ImageCapture_API> enter(image_capture, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->GetConfig(config, enter.callback()));
}

int32_t GetCameraCapabilities(PP_Resource image_capture,
                              PP_Resource* capabilities,
                              struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_ImageCapture_Private::GetCameraCapabilities()";
  EnterResource<PPB_ImageCapture_API> enter(image_capture, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->GetCameraCapabilities(
      capabilities,
      enter.callback()));
}

int32_t CaptureStillImage(
    PP_Resource image_capture,
    PPB_ImageCapture_Private_ShutterCallback shutter_callback,
    PPB_ImageCapture_Private_PreviewCallback preview_callback,
    PPB_ImageCapture_Private_JpegCallback jpeg_callback,
    int64_t* sequence_id) {
  VLOG(4) << "PPB_ImageCapture_Private::CaptureStillImage()";
  EnterResource<PPB_ImageCapture_API> enter(image_capture, true);
  if (enter.failed())
    return enter.retval();
  return enter.object()->CaptureStillImage(shutter_callback,
                                           preview_callback,
                                           jpeg_callback,
                                           sequence_id);
}

const PPB_ImageCapture_Private_0_1 g_ppb_imagecapture_private_thunk_0_1 = {
  &Create,
  &IsImageCapture,
  &Close,
  &SetConfig,
  &GetConfig,
  &GetCameraCapabilities,
  &CaptureStillImage
};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_ImageCapture_Private_0_1*
    GetPPB_ImageCapture_Private_0_1_Thunk() {
  return &g_ppb_imagecapture_private_thunk_0_1;
}

}  // namespace thunk
}  // namespace ppapi
