// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From dev/ppb_view_dev.idl modified Thu Mar 28 11:12:59 2013.

#include "ppapi/c/dev/ppb_view_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/ppb_view_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

float GetDeviceScale(PP_Resource resource) {
  VLOG(4) << "PPB_View_Dev::GetDeviceScale()";
  EnterResource<PPB_View_API> enter(resource, true);
  if (enter.failed())
    return 0.0f;
  return enter.object()->GetDeviceScale();
}

float GetCSSScale(PP_Resource resource) {
  VLOG(4) << "PPB_View_Dev::GetCSSScale()";
  EnterResource<PPB_View_API> enter(resource, true);
  if (enter.failed())
    return 0.0f;
  return enter.object()->GetCSSScale();
}

const PPB_View_Dev_0_1 g_ppb_view_dev_thunk_0_1 = {
  &GetDeviceScale,
  &GetCSSScale
};

}  // namespace

const PPB_View_Dev_0_1* GetPPB_View_Dev_0_1_Thunk() {
  return &g_ppb_view_dev_thunk_0_1;
}

}  // namespace thunk
}  // namespace ppapi
