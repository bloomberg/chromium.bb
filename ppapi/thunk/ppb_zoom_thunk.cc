// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/dev/ppb_zoom_dev.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_instance_api.h"

namespace ppapi {
namespace thunk {

namespace {

void ZoomChanged(PP_Instance instance, double factor) {
  EnterFunction<PPB_Instance_FunctionAPI> enter(instance, true);
  if (enter.succeeded())
    enter.functions()->ZoomChanged(instance, factor);
}

void ZoomLimitsChanged(PP_Instance instance,
                       double minimum_factor,
                       double maximum_factor) {
  EnterFunction<PPB_Instance_FunctionAPI> enter(instance, true);
  if (enter.succeeded()) {
    enter.functions()->ZoomLimitsChanged(instance,
                                         minimum_factor, maximum_factor);
  }
}

const PPB_Zoom_Dev g_ppb_zoom_thunk = {
  &ZoomChanged,
  &ZoomLimitsChanged
};

}  // namespace

const PPB_Zoom_Dev* GetPPB_Zoom_Dev_Thunk() {
  return &g_ppb_zoom_thunk;
}

}  // namespace thunk
}  // namespace ppapi
