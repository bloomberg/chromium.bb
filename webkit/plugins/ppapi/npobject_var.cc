// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/npobject_var.h"

#include "base/logging.h"
#include "ppapi/c/pp_var.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"
#include "webkit/plugins/ppapi/resource_tracker.h"

using WebKit::WebBindings;

namespace ppapi {

// NPObjectVar -----------------------------------------------------------------

NPObjectVar::NPObjectVar(PP_Module module,
                         PP_Instance instance,
                         NPObject* np_object)
    : Var(module),
      pp_instance_(instance),
      np_object_(np_object) {
  WebBindings::retainObject(np_object_);
  webkit::ppapi::ResourceTracker::Get()->AddNPObjectVar(this);
}

NPObjectVar::~NPObjectVar() {
  if (pp_instance())
    webkit::ppapi::ResourceTracker::Get()->RemoveNPObjectVar(this);
  WebBindings::releaseObject(np_object_);
}

NPObjectVar* NPObjectVar::AsNPObjectVar() {
  return this;
}

PP_Var NPObjectVar::GetPPVar() {
  int32 id = GetOrCreateVarID();
  if (!id)
    return PP_MakeNull();

  PP_Var result;
  result.type = PP_VARTYPE_OBJECT;
  result.value.as_id = id;
  return result;
}

PP_VarType NPObjectVar::GetType() const {
  return PP_VARTYPE_OBJECT;
}

void NPObjectVar::InstanceDeleted() {
  DCHECK(pp_instance_);
  pp_instance_ = 0;
}

// static
scoped_refptr<NPObjectVar> NPObjectVar::FromPPVar(PP_Var var) {
  if (var.type != PP_VARTYPE_OBJECT)
    return scoped_refptr<NPObjectVar>(NULL);
  scoped_refptr<Var> var_object(
      webkit::ppapi::ResourceTracker::Get()->GetVarTracker()->GetVar(var));
  if (!var_object)
    return scoped_refptr<NPObjectVar>();
  return scoped_refptr<NPObjectVar>(var_object->AsNPObjectVar());
}

}  // namespace ppapi
