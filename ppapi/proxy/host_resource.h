// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_HOST_RESOURCE_H_
#define PPAPI_PROXY_HOST_RESOURCE_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"

namespace pp {
namespace proxy {

// Represents a PP_Resource sent over the wire. This just wraps a PP_Resource.
// The point is to prevent mistakes where the wrong resource value is sent.
// Resource values are remapped in the plugin so that it can talk to multiple
// hosts. If all values were PP_Resource, it would be easy to forget to do
// this tranformation.
//
// All HostResources respresent IDs valid in the host.
class HostResource {
 public:
  HostResource() : instance_(0), host_resource_(0) {
  }

  bool is_null() const {
    return !host_resource_;
  }

  // Sets and retrieves the internal PP_Resource which is valid for the host
  // (a.k.a. renderer, as opposed to the plugin) process.
  //
  // DO NOT CALL THESE FUNCTIONS IN THE PLUGIN SIDE OF THE PROXY. The values
  // will be invalid. See the class comment above.
  void SetHostResource(PP_Instance instance, PP_Resource resource) {
    instance_ = instance;
    host_resource_ = resource;
  }
  PP_Resource host_resource() const {
    return host_resource_;
  }

  PP_Instance instance() const { return instance_; }

  // This object is used in maps so we need to provide this sorting operator.
  bool operator<(const HostResource& other) const {
    if (instance_ != other.instance_)
      return instance_ < other.instance_;
    return host_resource_ < other.host_resource_;
  }

 private:
  PP_Instance instance_;
  PP_Resource host_resource_;
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_HOST_RESOURCE_H_
