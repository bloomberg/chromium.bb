// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/dev/ppb_layer_compositor_dev.h"
#include "ppapi/thunk/common.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/ppb_layer_compositor_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance) {
  return 0;
}

PP_Bool IsLayerCompositor(PP_Resource resource) {
  return PP_FALSE;
}

PP_Bool AddLayer(PP_Resource compositor, PP_Resource layer) {
  return PP_FALSE;
}

void RemoveLayer(PP_Resource compositor, PP_Resource layer) {
}

void SetZIndex(PP_Resource compositor, PP_Resource layer, int32_t index) {
}

void SetRect(PP_Resource compositor, PP_Resource layer,
             const struct PP_Rect* rect) {
}

void SetDisplay(PP_Resource compositor, PP_Resource layer,
                PP_Bool is_displayed) {
}

void MarkAsDirty(PP_Resource compositor, PP_Resource layer) {
}

int32_t SwapBuffers(PP_Resource compositor,
                    struct PP_CompletionCallback callback) {
  return MayForceCallback(callback, PP_ERROR_FAILED);
}

const PPB_LayerCompositor_Dev g_ppb_layer_compositor_thunk = {
  &Create,
  &IsLayerCompositor,
  &AddLayer,
  &RemoveLayer,
  &SetZIndex,
  &SetRect,
  &SetDisplay,
  &MarkAsDirty,
  &SwapBuffers,
};

}  // namespace

const PPB_LayerCompositor_Dev* GetPPB_LayerCompositor_Dev_Thunk() {
  return &g_ppb_layer_compositor_thunk;
}

}  // namespace thunk
}  // namespace ppapi
