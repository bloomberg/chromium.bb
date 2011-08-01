// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/var.h"

#include <limits>

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/shared_impl/tracker_base.h"

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

      // Since this is for logging, escape NULLs, truncate length.
      std::string result;
      const size_t kTruncateAboveLength = 128;
      if (string->value().size() > kTruncateAboveLength)
        result = string->value().substr(0, kTruncateAboveLength) + "...";
      else
        result = string->value();

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
    if (!TrackerBase::Get()->AddRefVar(static_cast<int32>(var.value.as_id)))
      DLOG(WARNING) << "AddRefVar()ing a nonexistent string/object var.";
  }
}

// static
void Var::PluginReleasePPVar(PP_Var var) {
  if (var.type == PP_VARTYPE_STRING || var.type == PP_VARTYPE_OBJECT) {
    if (!TrackerBase::Get()->UnrefVar(static_cast<int32>(var.value.as_id)))
      DLOG(WARNING) << "ReleaseVar()ing a nonexistent string/object var.";
  }
}

StringVar* Var::AsStringVar() {
  return NULL;
}

NPObjectVar* Var::AsNPObjectVar() {
  return NULL;
}

int32 Var::GetExistingVarID() const {
  return var_id_;
}

int32 Var::GetOrCreateVarID() {
  TrackerBase* tracker = TrackerBase::Get();
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
      TrackerBase::Get()->GetVar(static_cast<int32>(var.value.as_id)));
  if (!var_object)
    return scoped_refptr<StringVar>();
  return scoped_refptr<StringVar>(var_object->AsStringVar());
}

}  // namespace ppapi

