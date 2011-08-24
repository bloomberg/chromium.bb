// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_layer_compositor_impl.h"

#include "ppapi/c/pp_errors.h"
#include "webkit/plugins/ppapi/common.h"

using ppapi::thunk::PPB_LayerCompositor_API;

namespace webkit {
namespace ppapi {

PPB_LayerCompositor_Impl::PPB_LayerCompositor_Impl(PP_Instance instance)
    : Resource(instance) {
}

PPB_LayerCompositor_Impl::~PPB_LayerCompositor_Impl() {
}

PPB_LayerCompositor_API*
PPB_LayerCompositor_Impl::AsPPB_LayerCompositor_API() {
  return this;
}

PP_Bool PPB_LayerCompositor_Impl::AddLayer(PP_Resource layer) {
  return PP_FALSE;
}

void PPB_LayerCompositor_Impl::RemoveLayer(PP_Resource layer) {
}

void PPB_LayerCompositor_Impl::SetZIndex(PP_Resource layer, int32_t index) {
}

void PPB_LayerCompositor_Impl::SetRect(PP_Resource layer,
                                       const PP_Rect* rect) {
}

void PPB_LayerCompositor_Impl::SetDisplay(PP_Resource layer,
                                          PP_Bool is_displayed) {
}

void PPB_LayerCompositor_Impl::MarkAsDirty(PP_Resource layer) {
}

int32_t PPB_LayerCompositor_Impl::SwapBuffers(PP_CompletionCallback callback) {
  return PP_ERROR_FAILED;
}

}  // namespace ppapi
}  // namespace webkit
