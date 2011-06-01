// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_ENTER_PROXY_H_
#define PPAPI_PROXY_ENTER_PROXY_H_

#include "base/logging.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/thunk/enter.h"

namespace pp {
namespace proxy {

// Wrapper around EnterResourceNoLock that takes a host resource. This is used
// when handling messages in the plugin from the host and we need to convert to
// an object in the plugin side corresponding to that.
//
// This never locks since we assume the host Resource is coming from IPC, and
// never logs errors since we assume the host is doing reasonable things.
template<typename ResourceT>
class EnterPluginFromHostResource
    : public ::ppapi::thunk::EnterResourceNoLock<ResourceT> {
 public:
  EnterPluginFromHostResource(const HostResource& host_resource)
      : ::ppapi::thunk::EnterResourceNoLock<ResourceT>(
            PluginResourceTracker::GetInstance()->PluginResourceForHostResource(
                host_resource),
            false) {
    // Validate that we're in the plugin rather than the host. Otherwise this
    // object will do the wrong thing. In the plugin, the instance should have
    // a corresponding plugin dispatcher (assuming the resource is valid).
    DCHECK(this->failed() ||
           PluginDispatcher::GetForInstance(host_resource.instance()));
  }
};

template<typename ResourceT>
class EnterHostFromHostResource
    : public ::ppapi::thunk::EnterResourceNoLock<ResourceT> {
 public:
  EnterHostFromHostResource(const HostResource& host_resource)
      : ::ppapi::thunk::EnterResourceNoLock<ResourceT>(
            host_resource.host_resource(), false) {
    // Validate that we're in the host rather than the plugin. Otherwise this
    // object will do the wrong thing. In the host, the instance should have
    // a corresponding host disptacher (assuming the resource is valid).
    DCHECK(this->failed() ||
           HostDispatcher::GetForInstance(host_resource.instance()));
  }
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_ENTER_PROXY_H_
