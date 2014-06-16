// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/compositor_resource.h"

namespace ppapi {
namespace proxy {

CompositorResource::CompositorResource(Connection connection,
                                       PP_Instance instance)
    : PluginResource(connection, instance) {
}

CompositorResource::~CompositorResource() {
}

thunk::PPB_Compositor_API* CompositorResource::AsPPB_Compositor_API() {
  return this;
}

PP_Resource CompositorResource::AddLayer() {
  return 0;
}

int32_t CompositorResource::CommitLayers(
    const scoped_refptr<ppapi::TrackedCallback>& callback) {
  return PP_ERROR_NOTSUPPORTED;
}

int32_t CompositorResource::ResetLayers() {
  return PP_ERROR_NOTSUPPORTED;
}

}  // namespace proxy
}  // namespace ppapi
