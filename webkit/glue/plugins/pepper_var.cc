// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_var.h"

#include "third_party/ppapi/c/pp_var.h"
#include "third_party/ppapi/c/ppb_var.h"
#include "webkit/glue/plugins/pepper_string.h"

namespace pepper {

namespace {

void AddRef(PP_Var var) {
  if (var.type == PP_VarType_String) {
    reinterpret_cast<String*>(var.value.as_id)->AddRef();
  } else if (var.type == PP_VarType_Object) {
    // TODO(implement objects).
  }
}

void Release(PP_Var var) {
  if (var.type == PP_VarType_String) {
    reinterpret_cast<String*>(var.value.as_id)->Release();
  } else if (var.type == PP_VarType_Object) {
    // TODO(implement objects).
  }
}

PP_Var VarFromUtf8(const char* data, uint32_t len) {
  PP_Var ret;
  ret.type = PP_VarType_String;
  String* str = new String(data, len);
  str->AddRef();  // This is for the caller, we return w/ a refcount of 1.
  return ret;
}

const char* VarToUtf8(PP_Var var, uint32_t* len) {
  if (var.type != PP_VarType_String) {
    *len = 0;
    return NULL;
  }
  const std::string& str =
      reinterpret_cast<const String*>(var.value.as_id)->value();
  *len = static_cast<uint32_t>(str.size());
  if (str.empty())
    return "";  // Don't return NULL on success.
  return str.data();
}

bool HasProperty(PP_Var object,
                 PP_Var name,
                 PP_Var* exception) {
  // TODO(brettw) implement this.
  return false;
}

PP_Var GetProperty(PP_Var object,
                   PP_Var name,
                   PP_Var* exception) {
  // TODO(brettw) implement this.
  PP_Var ret;
  ret.type = PP_VarType_Void;
  return ret;
}

void GetAllPropertyNames(PP_Var object,
                         uint32_t* property_count,
                         PP_Var** properties,
                         PP_Var* exception) {
  // TODO(brettw) implement this.
}

void SetProperty(PP_Var object,
                 PP_Var name,
                 PP_Var value,
                 PP_Var* exception) {
  // TODO(brettw) implement this.
}

void RemoveProperty(PP_Var object,
                    PP_Var name,
                    PP_Var* exception) {
  // TODO(brettw) implement this.
}

PP_Var Call(PP_Var object,
            PP_Var method_name,
            int32_t argc,
            PP_Var* argv,
            PP_Var* exception) {
  // TODO(brettw) implement this.
  PP_Var ret;
  ret.type = PP_VarType_Void;
  return ret;
}

PP_Var Construct(PP_Var object,
                 int32_t argc,
                 PP_Var* argv,
                 PP_Var* exception) {
  // TODO(brettw) implement this.
  PP_Var ret;
  ret.type = PP_VarType_Void;
  return ret;
}

PP_Var CreateObject(const PPP_Class* object_class,
                    void* object_data) {
  // TODO(brettw) implement this.
  PP_Var ret;
  ret.type = PP_VarType_Void;
  return ret;
}

const PPB_Var var_interface = {
  &AddRef,
  &Release,
  &VarFromUtf8,
  &VarToUtf8,
  &HasProperty,
  &GetProperty,
  &GetAllPropertyNames,
  &SetProperty,
  &RemoveProperty,
  &Call,
  &Construct,
  &CreateObject
};

}  // namespace

const PPB_Var* GetVarInterface() {
  return &var_interface;
}

}  // namespace pepper
