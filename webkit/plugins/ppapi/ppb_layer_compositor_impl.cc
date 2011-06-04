// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_layer_compositor_impl.h"

#include "ppapi/c/pp_errors.h"
#include "webkit/plugins/ppapi/common.h"

namespace webkit {
namespace ppapi {

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
  return PP_ERROR_FAILED;
}

const PPB_LayerCompositor_Dev ppb_layercompositor = {
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

PPB_LayerCompositor_Impl::PPB_LayerCompositor_Impl(PluginInstance* instance)
    : Resource(instance) {
}

PPB_LayerCompositor_Impl::~PPB_LayerCompositor_Impl() {
}

PPB_LayerCompositor_Impl*
PPB_LayerCompositor_Impl::AsPPB_LayerCompositor_Impl() {
  return this;
}

// static
const PPB_LayerCompositor_Dev* PPB_LayerCompositor_Impl::GetInterface() {
  return &ppb_layercompositor;
}

PP_Bool PPB_LayerCompositor_Impl::AddLayer(PP_Resource layer) {
  return PP_FALSE;
}

void PPB_LayerCompositor_Impl::RemoveLayer(PP_Resource layer) {
}

void PPB_LayerCompositor_Impl::SetZIndex(PP_Resource layer, int32_t index) {
}

void PPB_LayerCompositor_Impl::SetRect(PP_Resource layer,
                                       const struct PP_Rect* rect) {
}

void PPB_LayerCompositor_Impl::SetDisplay(PP_Resource layer,
                                          PP_Bool is_displayed) {
}

void PPB_LayerCompositor_Impl::MarkAsDirty(PP_Resource layer) {
}

int32_t PPB_LayerCompositor_Impl::SwapBuffers(
    struct PP_CompletionCallback callback) {
  return PP_ERROR_FAILED;
}

}  // namespace ppapi
}  // namespace webkit
