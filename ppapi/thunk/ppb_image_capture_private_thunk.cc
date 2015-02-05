// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From private/ppb_image_capture_private.idl modified Thu Feb  5 22:47:43 2015.

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
                   void* user_data) {
  VLOG(4) << "PPB_ImageCapture_Private::Create()";
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateImageCapturePrivate(
      instance, camera_source_id, user_data);
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

int32_t GetCameraCapabilities(PP_Resource image_capture,
                              PP_Resource* capabilities,
                              struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_ImageCapture_Private::GetCameraCapabilities()";
  EnterResource<PPB_ImageCapture_API> enter(image_capture, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(
      enter.object()->GetCameraCapabilities(capabilities, enter.callback()));
}

const PPB_ImageCapture_Private_0_1 g_ppb_imagecapture_private_thunk_0_1 =
    {&Create, &IsImageCapture, &Close, &GetCameraCapabilities};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_ImageCapture_Private_0_1*
GetPPB_ImageCapture_Private_0_1_Thunk() {
  return &g_ppb_imagecapture_private_thunk_0_1;
}

}  // namespace thunk
}  // namespace ppapi
