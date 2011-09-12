// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_errors.h"
#include "ppapi/thunk/common.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/ppb_video_capture_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

namespace {

typedef EnterResource<PPB_VideoCapture_API> EnterVideoCapture;

PP_Resource Create(PP_Instance instance) {
  EnterFunction<ResourceCreationAPI> enter(instance, true);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateVideoCapture(instance);
}

PP_Bool IsVideoCapture(PP_Resource resource) {
  EnterVideoCapture enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t StartCapture(PP_Resource video_capture,
                     const PP_VideoCaptureDeviceInfo_Dev* requested_info,
                     uint32_t buffer_count) {
  EnterVideoCapture enter(video_capture, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;

  return enter.object()->StartCapture(*requested_info, buffer_count);
}

int32_t ReuseBuffer(PP_Resource video_capture,
                    uint32_t buffer) {
  EnterVideoCapture enter(video_capture, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;

  return enter.object()->ReuseBuffer(buffer);
}

int32_t StopCapture(PP_Resource video_capture) {
  EnterVideoCapture enter(video_capture, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;

  return enter.object()->StopCapture();
}

const PPB_VideoCapture_Dev g_ppb_videocapture_thunk = {
  &Create,
  &IsVideoCapture,
  &StartCapture,
  &ReuseBuffer,
  &StopCapture
};

}  // namespace

const PPB_VideoCapture_Dev* GetPPB_VideoCapture_Dev_Thunk() {
  return &g_ppb_videocapture_thunk;
}

}  // namespace thunk
}  // namespace ppapi
