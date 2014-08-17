// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From private/ppb_image_capture_config_private.idl,
//   modified Wed Aug 13 14:07:52 2014.

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_image_capture_config_private.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"
#include "ppapi/thunk/ppb_image_capture_config_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance) {
  VLOG(4) << "PPB_ImageCaptureConfig_Private::Create()";
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateImageCaptureConfigPrivate(instance);
}

PP_Bool IsImageCaptureConfig(PP_Resource resource) {
  VLOG(4) << "PPB_ImageCaptureConfig_Private::IsImageCaptureConfig()";
  EnterResource<PPB_ImageCaptureConfig_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

void GetPreviewSize(PP_Resource config, struct PP_Size* preview_size) {
  VLOG(4) << "PPB_ImageCaptureConfig_Private::GetPreviewSize()";
  EnterResource<PPB_ImageCaptureConfig_API> enter(config, true);
  if (enter.failed())
    return;
  enter.object()->GetPreviewSize(preview_size);
}

void SetPreviewSize(PP_Resource config, const struct PP_Size* preview_size) {
  VLOG(4) << "PPB_ImageCaptureConfig_Private::SetPreviewSize()";
  EnterResource<PPB_ImageCaptureConfig_API> enter(config, true);
  if (enter.failed())
    return;
  enter.object()->SetPreviewSize(preview_size);
}

void GetJpegSize(PP_Resource config, struct PP_Size* jpeg_size) {
  VLOG(4) << "PPB_ImageCaptureConfig_Private::GetJpegSize()";
  EnterResource<PPB_ImageCaptureConfig_API> enter(config, true);
  if (enter.failed())
    return;
  enter.object()->GetJpegSize(jpeg_size);
}

void SetJpegSize(PP_Resource config, const struct PP_Size* jpeg_size) {
  VLOG(4) << "PPB_ImageCaptureConfig_Private::SetJpegSize()";
  EnterResource<PPB_ImageCaptureConfig_API> enter(config, true);
  if (enter.failed())
    return;
  enter.object()->SetJpegSize(jpeg_size);
}

const PPB_ImageCaptureConfig_Private_0_1
    g_ppb_imagecaptureconfig_private_thunk_0_1 = {
  &Create,
  &IsImageCaptureConfig,
  &GetPreviewSize,
  &SetPreviewSize,
  &GetJpegSize,
  &SetJpegSize
};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_ImageCaptureConfig_Private_0_1*
    GetPPB_ImageCaptureConfig_Private_0_1_Thunk() {
  return &g_ppb_imagecaptureconfig_private_thunk_0_1;
}

}  // namespace thunk
}  // namespace ppapi
