// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/resource_var.h"

namespace ppapi {

ResourceVar::ResourceVar() : pp_resource_(0) {}

ResourceVar::ResourceVar(PP_Resource pp_resource) : pp_resource_(pp_resource) {}

ResourceVar::ResourceVar(const IPC::Message& creation_message)
    : pp_resource_(0),
      creation_message_(creation_message) {}

ResourceVar::~ResourceVar() {}

ResourceVar* ResourceVar::AsResourceVar() {
  return this;
}

PP_VarType ResourceVar::GetType() const {
  // TODO(mgiuca): Return PP_VARTYPE_RESOURCE, once that is a valid enum value.
  NOTREACHED();
  return PP_VARTYPE_UNDEFINED;
}

// static
ResourceVar* ResourceVar::FromPPVar(PP_Var var) {
  // TODO(mgiuca): Implement this function, once PP_VARTYPE_RESOURCE is
  // introduced.
  return NULL;
}

}  // namespace ppapi
