// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_COMPOSITOR_LAYER_RESOURCE_H_
#define PPAPI_PROXY_COMPOSITOR_LAYER_RESOURCE_H_

#include "ppapi/c/ppb_compositor_layer.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/thunk/ppb_compositor_layer_api.h"

namespace ppapi {
namespace proxy {

class PPAPI_PROXY_EXPORT CompositorLayerResource
    : public PluginResource,
      public thunk::PPB_CompositorLayer_API {
 public:
  CompositorLayerResource(Connection connection,
                     PP_Instance instance);

 private:
  virtual ~CompositorLayerResource();

  // Resource overrides:
  virtual thunk::PPB_CompositorLayer_API* AsPPB_CompositorLayer_API() OVERRIDE;

  // thunk::PPB_Compositor_API overrides:
  virtual int32_t SetColor(float red,
                           float green,
                           float blue,
                           float alpha,
                           const PP_Size* size) OVERRIDE;
  virtual int32_t SetTexture(
      PP_Resource context,
      uint32_t texture,
      const PP_Size* size,
      const scoped_refptr<ppapi::TrackedCallback>& callback) OVERRIDE;
  virtual int32_t SetImage(
      PP_Resource image_data,
      const PP_Size* size,
      const scoped_refptr<ppapi::TrackedCallback>& callback) OVERRIDE;
  virtual int32_t SetClipRect(const PP_Rect* rect) OVERRIDE;
  virtual int32_t SetTransform(const float matrix[16]) OVERRIDE;
  virtual int32_t SetOpacity(float opacity) OVERRIDE;
  virtual int32_t SetBlendMode(PP_BlendMode mode) OVERRIDE;
  virtual int32_t SetSourceRect(const PP_FloatRect* rect) OVERRIDE;
  virtual int32_t SetPremultipliedAlpha(PP_Bool premult) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(CompositorLayerResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_COMPOSITOR_LAYER_RESOURCE_H_
