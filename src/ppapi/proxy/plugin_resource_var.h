// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PLUGIN_RESOURCE_VAR_H_
#define PPAPI_PROXY_PLUGIN_RESOURCE_VAR_H_

#include "base/macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/resource_var.h"
#include "ppapi/shared_impl/var.h"

// Represents a resource Var, usable on the plugin side.
class PPAPI_PROXY_EXPORT PluginResourceVar : public ppapi::ResourceVar {
 public:
  // Makes a null resource var.
  PluginResourceVar();

  // Makes a resource var with an existing resource.
  // Takes one reference to the given resource.
  explicit PluginResourceVar(ppapi::Resource* resource);

  // ResourceVar override.
  PP_Resource GetPPResource() const override;
  bool IsPending() const override;

  scoped_refptr<ppapi::Resource> resource() const { return resource_; }

 protected:
  ~PluginResourceVar() override;

 private:
  // If NULL, this represents the PP_Resource 0.
  scoped_refptr<ppapi::Resource> resource_;

  DISALLOW_COPY_AND_ASSIGN(PluginResourceVar);
};

#endif
