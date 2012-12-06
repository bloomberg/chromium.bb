// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_HOST_RESOURCE_H_
#define PPAPI_SHARED_IMPL_HOST_RESOURCE_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

// Represents a PP_Resource sent over the wire. This just wraps a PP_Resource.
// The point is to prevent mistakes where the wrong resource value is sent.
// Resource values are remapped in the plugin so that it can talk to multiple
// hosts. If all values were PP_Resource, it would be easy to forget to do
// this transformation.
//
// To get the corresponding plugin PP_Resource for a HostResource, use
// PluginResourceTracker::PluginResourceForHostResource().
//
// All HostResources respresent IDs valid in the host.
class PPAPI_SHARED_EXPORT HostResource {
 public:
  HostResource();

  bool is_null() const {
    return !host_resource_;
  }

  // Some resources are plugin-side only and don't have a corresponding
  // resource in the host. Yet these resources still need an instance to be
  // associated with. This function creates a HostResource with the given
  // instances and a 0 host resource ID for these cases.
  static HostResource MakeInstanceOnly(PP_Instance instance);

  // Sets and retrieves the internal PP_Resource which is valid for the host
  // (a.k.a. renderer, as opposed to the plugin) process.
  //
  // DO NOT CALL THESE FUNCTIONS IN THE PLUGIN SIDE OF THE PROXY. The values
  // will be invalid. See the class comment above.
  void SetHostResource(PP_Instance instance, PP_Resource resource);
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

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_HOST_RESOURCE_H_
