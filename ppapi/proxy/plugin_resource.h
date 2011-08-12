// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PLUGIN_RESOURCE_H_
#define PPAPI_PROXY_PLUGIN_RESOURCE_H_

#include "base/basictypes.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/host_resource.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/shared_impl/resource_object_base.h"

namespace pp {
namespace proxy {

class PluginResource : public ::ppapi::ResourceObjectBase {
 public:
  PluginResource(const HostResource& resource);
  virtual ~PluginResource();

  PP_Instance instance() const { return host_resource_.instance(); }

  // Returns the host resource ID for sending to the host process.
  const HostResource& host_resource() const {
    return host_resource_;
  }

  PluginDispatcher* GetDispatcher();

 private:
  // The resource ID in the host that this object corresponds to. Inside the
  // plugin we'll remap the resource IDs so we can have many host processes
  // each independently generating resources (which may conflict) but the IDs
  // in the plugin will all be unique.
  HostResource host_resource_;

  DISALLOW_COPY_AND_ASSIGN(PluginResource);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_PLUGIN_RESOURCE_H_
