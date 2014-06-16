// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_COMPOSITOR_RESOURCE_H_
#define PPAPI_PROXY_COMPOSITOR_RESOURCE_H_

#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/thunk/ppb_compositor_api.h"

namespace ppapi {
namespace proxy {

class PPAPI_PROXY_EXPORT CompositorResource
    : public PluginResource,
      public thunk::PPB_Compositor_API {
 public:
  CompositorResource(Connection connection,
                     PP_Instance instance);

 private:
  virtual ~CompositorResource();

  // Resource overrides:
  virtual thunk::PPB_Compositor_API* AsPPB_Compositor_API() OVERRIDE;

  // thunk::PPB_Compositor_API overrides:
  virtual PP_Resource AddLayer() OVERRIDE;
  virtual int32_t CommitLayers(
      const scoped_refptr<ppapi::TrackedCallback>& callback) OVERRIDE;
  virtual int32_t ResetLayers() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(CompositorResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_COMPOSITOR_RESOURCE_H_
