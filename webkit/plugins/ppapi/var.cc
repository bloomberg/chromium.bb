// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/var.h"

#include <limits>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/pp_var.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/plugin_object.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/resource_tracker.h"

using WebKit::WebBindings;

namespace webkit {
namespace ppapi {

// Var -------------------------------------------------------------------------

Var::Var(PP_Module module) : pp_module_(module), var_id_(0) {
}

Var::~Var() {
}

// static
std::string Var::PPVarToLogString(PP_Var var) {
  switch (var.type) {
    case PP_VARTYPE_UNDEFINED:
      return "[Undefined]";
    case PP_VARTYPE_NULL:
      return "[Null]";
    case PP_VARTYPE_BOOL:
      return var.value.as_bool ? "[True]" : "[False]";
    case PP_VARTYPE_INT32:
      return base::IntToString(var.value.as_int);
    case PP_VARTYPE_DOUBLE:
      return base::DoubleToString(var.value.as_double);
    case PP_VARTYPE_STRING: {
      scoped_refptr<StringVar> string(StringVar::FromPPVar(var));
      if (!string)
        return "[Invalid string]";

      // Since this is for logging, escape NULLs.
      std::string result = string->value();
      std::string null;
      null.push_back(0);
      ReplaceSubstringsAfterOffset(&result, 0, null, "\\0");
      return result;
    }
    case PP_VARTYPE_OBJECT:
      return "[Object]";
    default:
      return "[Invalid var]";
  }
}

// static
void Var::PluginAddRefPPVar(PP_Var var) {
  if (var.type == PP_VARTYPE_STRING || var.type == PP_VARTYPE_OBJECT) {
    if (!ResourceTracker::Get()->AddRefVar(static_cast<int32>(var.value.as_id)))
      DLOG(WARNING) << "AddRefVar()ing a nonexistent string/object var.";
  }
}

// static
void Var::PluginReleasePPVar(PP_Var var) {
  if (var.type == PP_VARTYPE_STRING || var.type == PP_VARTYPE_OBJECT) {
    if (!ResourceTracker::Get()->UnrefVar(static_cast<int32>(var.value.as_id)))
      DLOG(WARNING) << "ReleaseVar()ing a nonexistent string/object var.";
  }
}

StringVar* Var::AsStringVar() {
  return NULL;
}

ObjectVar* Var::AsObjectVar() {
  return NULL;
}

int32 Var::GetExistingVarID() const {
  return var_id_;
}

int32 Var::GetOrCreateVarID() {
  ResourceTracker *tracker = ResourceTracker::Get();
  if (var_id_) {
    if (!tracker->AddRefVar(var_id_))
      return 0;
  } else {
    var_id_ = tracker->AddVar(this);
    if (!var_id_)
      return 0;
  }
  return var_id_;
}

// StringVar -------------------------------------------------------------------

StringVar::StringVar(PP_Module module, const char* str, uint32 len)
    : Var(module),
      value_(str, len) {
}

StringVar::~StringVar() {
}

StringVar* StringVar::AsStringVar() {
  return this;
}

PP_Var StringVar::GetPPVar() {
  int32 id = GetOrCreateVarID();
  if (!id)
    return PP_MakeNull();

  PP_Var result;
  result.type = PP_VARTYPE_STRING;
  result.value.as_id = id;
  return result;
}

// static
PP_Var StringVar::StringToPPVar(PP_Module module, const std::string& var) {
  return StringToPPVar(module, var.c_str(), var.size());
}

// static
PP_Var StringVar::StringToPPVar(PP_Module module,
                                const char* data, uint32 len) {
  scoped_refptr<StringVar> str(new StringVar(module, data, len));
  if (!str || !IsStringUTF8(str->value()))
    return PP_MakeNull();
  return str->GetPPVar();
}

// static
scoped_refptr<StringVar> StringVar::FromPPVar(PP_Var var) {
  if (var.type != PP_VARTYPE_STRING)
    return scoped_refptr<StringVar>();
  scoped_refptr<Var> var_object(
      ResourceTracker::Get()->GetVar(static_cast<int32>(var.value.as_id)));
  if (!var_object)
    return scoped_refptr<StringVar>();
  return scoped_refptr<StringVar>(var_object->AsStringVar());
}

// ObjectVar -------------------------------------------------------------------

ObjectVar::ObjectVar(PP_Module module,
                     PP_Instance instance,
                     NPObject* np_object)
    : Var(module),
      pp_instance_(instance),
      np_object_(np_object) {
  WebBindings::retainObject(np_object_);
  ResourceTracker::Get()->AddNPObjectVar(this);
}

ObjectVar::~ObjectVar() {
  if (pp_instance_)
    ResourceTracker::Get()->RemoveNPObjectVar(this);

  WebBindings::releaseObject(np_object_);
}

ObjectVar* ObjectVar::AsObjectVar() {
  return this;
}

PP_Var ObjectVar::GetPPVar() {
  int32 id = GetOrCreateVarID();
  if (!id)
    return PP_MakeNull();

  PP_Var result;
  result.type = PP_VARTYPE_OBJECT;
  result.value.as_id = id;
  return result;
}

void ObjectVar::InstanceDeleted() {
  DCHECK(pp_instance_);
  pp_instance_ = NULL;
}

// static
scoped_refptr<ObjectVar> ObjectVar::FromPPVar(PP_Var var) {
  if (var.type != PP_VARTYPE_OBJECT)
    return scoped_refptr<ObjectVar>(NULL);
  scoped_refptr<Var> var_object(
      ResourceTracker::Get()->GetVar(static_cast<int32>(var.value.as_id)));
  if (!var_object)
    return scoped_refptr<ObjectVar>();
  return scoped_refptr<ObjectVar>(var_object->AsObjectVar());
}

// TryCatch --------------------------------------------------------------------

TryCatch::TryCatch(PP_Module module, PP_Var* exception)
    : pp_module_(module),
      has_exception_(exception && exception->type != PP_VARTYPE_UNDEFINED),
      exception_(exception) {
  WebBindings::pushExceptionHandler(&TryCatch::Catch, this);
}

TryCatch::~TryCatch() {
  WebBindings::popExceptionHandler();
}

void TryCatch::SetException(const char* message) {
  if (!pp_module_) {
    // Don't have a module to make the string.
    SetInvalidObjectException();
    return;
  }

  if (!has_exception()) {
    has_exception_ = true;
    if (exception_) {
      *exception_ = StringVar::StringToPPVar(pp_module_,
                                             message, strlen(message));
    }
  }
}

void TryCatch::SetInvalidObjectException() {
  if (!has_exception()) {
    has_exception_ = true;
    // TODO(brettw) bug 54504: Have a global singleton string that can hold
    // a generic error message.
    if (exception_)
      *exception_ = PP_MakeInt32(1);
  }
}

// static
void TryCatch::Catch(void* self, const char* message) {
  static_cast<TryCatch*>(self)->SetException(message);
}

}  // namespace ppapi
}  // namespace webkit

