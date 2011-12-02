// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_RESOURCE_HELPER_H_
#define WEBKIT_PLUGINS_PPAPI_RESOURCE_HELPER_H_

#include "base/basictypes.h"
#include "ppapi/c/pp_resource.h"
#include "webkit/plugins/webkit_plugins_export.h"

namespace ppapi {
class Resource;
}

namespace webkit {
namespace ppapi {

class PluginInstance;
class PluginModule;
class PluginDelegate;

// Helper functions for Resoruce implementations.
//
// This is specifically not designed to be a base class that derives from
// ppapi::Resource to avoid diamond inheritance if most of a resource class
// is implemented in the shared_impl (to share code with the proxy).
class ResourceHelper {
 public:
  // Returns the instance implementation object for the given resource, or NULL
  // if the resource has outlived its instance.
  static PluginInstance* GetPluginInstance(const ::ppapi::Resource* resource);

  // Returns the module for the given resource, or NULL if the resource has
  // outlived its instance.
  WEBKIT_PLUGINS_EXPORT static PluginModule* GetPluginModule(
      const ::ppapi::Resource* resource);

  // Returns the plugin delegate for the given resource, or NULL if the
  // resource has outlived its instance.
  static PluginDelegate* GetPluginDelegate(const ::ppapi::Resource* resource);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ResourceHelper);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_RESOURCE_IMPL_HELPER_H_
