// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/plugin_resource.h"

namespace pp {
namespace proxy {

PluginResource::PluginResource(const HostResource& resource)
    : host_resource_(resource) {
}

PluginResource::~PluginResource() {
}

#define DEFINE_TYPE_GETTER(RESOURCE)                    \
  RESOURCE* PluginResource::As##RESOURCE() { return NULL; }
FOR_ALL_PLUGIN_RESOURCES(DEFINE_TYPE_GETTER)
#undef DEFINE_TYPE_GETTER

}  // namespace proxy
}  // namespace pp
