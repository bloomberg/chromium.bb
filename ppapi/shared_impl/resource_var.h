// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_RESOURCE_VAR_H_
#define PPAPI_SHARED_IMPL_RESOURCE_VAR_H_

#include "ipc/ipc_message.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {

// Represents a resource Var.
class PPAPI_SHARED_EXPORT ResourceVar : public Var {
 public:
  // Makes a null resource var.
  ResourceVar();

  // Makes a resource var with an existing plugin-side resource.
  explicit ResourceVar(PP_Resource pp_resource);

  // Makes a resource var with a pending resource host.
  // The |creation_message| contains instructions on how to create the
  // plugin-side resource. Its type depends on the type of resource.
  explicit ResourceVar(const IPC::Message& creation_message);

  virtual ~ResourceVar();

  // Gets the resource ID associated with this var.
  // This is 0 if a resource is still pending.
  PP_Resource pp_resource() const { return pp_resource_; }

  // Gets the message for creating a plugin-side resource.
  // May be an empty message.
  const IPC::Message& creation_message() const { return creation_message_; }

  // Var override.
  virtual ResourceVar* AsResourceVar() OVERRIDE;
  virtual PP_VarType GetType() const OVERRIDE;

  // Helper function that converts a PP_Var to a ResourceVar. This will
  // return NULL if the PP_Var is not of Resource type.
  static ResourceVar* FromPPVar(PP_Var var);

 private:
  // Real resource ID in the plugin. 0 if one has not yet been created
  // (indicating that there is a pending host resource).
  PP_Resource pp_resource_;

  // If the plugin-side resource has not yet been created, carries a message to
  // create a resource of the specific type on the plugin side.
  // Otherwise, carries an empty message.
  IPC::Message creation_message_;

  DISALLOW_COPY_AND_ASSIGN(ResourceVar);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_RESOURCE_VAR_H_
