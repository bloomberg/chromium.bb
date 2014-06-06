// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/compositor_layer_resource.h"

namespace ppapi {
namespace proxy {

CompositorLayerResource::CompositorLayerResource(Connection connection,
                                       PP_Instance instance)
    : PluginResource(connection, instance) {
}

CompositorLayerResource::~CompositorLayerResource() {
}

thunk::PPB_CompositorLayer_API*
CompositorLayerResource::AsPPB_CompositorLayer_API() {
  return this;
}

int32_t CompositorLayerResource::SetColor(float red,
                                          float green,
                                          float blue,
                                          float alpha,
                                          const PP_Size* size) {
  return PP_ERROR_NOTSUPPORTED;
}

int32_t CompositorLayerResource::SetTexture(
    PP_Resource context,
    uint32_t texture,
    const PP_Size* size,
    const scoped_refptr<ppapi::TrackedCallback>& callback) {
  return PP_ERROR_NOTSUPPORTED;
}

int32_t CompositorLayerResource::SetImage(
    PP_Resource image_data,
    const PP_Size* size,
    const scoped_refptr<ppapi::TrackedCallback>& callback) {
  return PP_ERROR_NOTSUPPORTED;
}

int32_t CompositorLayerResource::SetClipRect(const PP_Rect* rect) {
  return PP_ERROR_NOTSUPPORTED;
}

int32_t CompositorLayerResource::SetTransform(const float matrix[16]) {
  return PP_ERROR_NOTSUPPORTED;
}

int32_t CompositorLayerResource::SetOpacity(float opacity) {
  return PP_ERROR_NOTSUPPORTED;
}

int32_t CompositorLayerResource::SetBlendMode(PP_BlendMode mode) {
  return PP_ERROR_NOTSUPPORTED;
}

int32_t CompositorLayerResource::SetSourceRect(
    const PP_FloatRect* rect) {
  return PP_ERROR_NOTSUPPORTED;
}

int32_t CompositorLayerResource::SetPremultipliedAlpha(PP_Bool premult) {
  return PP_ERROR_NOTSUPPORTED;
}

}  // namespace proxy
}  // namespace ppapi
