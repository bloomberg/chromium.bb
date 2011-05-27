// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/var.h"

#include <stdio.h>
#include <string.h>

#include <algorithm>

#include "ppapi/c/pp_var.h"
#ifndef PPAPI_VAR_REMOVE_SCRIPTING
#  include "ppapi/c/dev/ppb_var_deprecated.h"
#endif
#include "ppapi/c/ppb_var.h"
#include "ppapi/cpp/common.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/dev/scriptable_object_deprecated.h"

// Define equivalent to snprintf on Windows.
#if defined(_MSC_VER)
#  define snprintf sprintf_s
#endif

namespace pp {

namespace {

template <> const char* interface_name<PPB_Var>() {
  return PPB_VAR_INTERFACE;
}

#ifdef PPAPI_VAR_REMOVE_SCRIPTING
typedef PPB_Var PPB_Var_Interface_Type;
#else
typedef PPB_Var_Deprecated PPB_Var_Interface_Type;

template <> const char* interface_name<PPB_Var_Deprecated>() {
  return PPB_VAR_DEPRECATED_INTERFACE;
}
#endif

// Technically you can call AddRef and Release on any Var, but it may involve
// cross-process calls depending on the plugin. This is an optimization so we
// only do refcounting on the necessary objects.
inline bool NeedsRefcounting(const PP_Var& var) {
  return var.type > PP_VARTYPE_DOUBLE;
}

}  // namespace

using namespace deprecated;

Var::Var() {
  var_.type = PP_VARTYPE_UNDEFINED;
  var_.padding = 0;
  needs_release_ = false;
}

Var::Var(Null) {
  var_.type = PP_VARTYPE_NULL;
  var_.padding = 0;
  needs_release_ = false;
}

Var::Var(bool b) {
  var_.type = PP_VARTYPE_BOOL;
  var_.padding = 0;
  var_.value.as_bool = BoolToPPBool(b);
  needs_release_ = false;
}

Var::Var(int32_t i) {
  var_.type = PP_VARTYPE_INT32;
  var_.padding = 0;
  var_.value.as_int = i;
  needs_release_ = false;
}

Var::Var(double d) {
  var_.type = PP_VARTYPE_DOUBLE;
  var_.padding = 0;
  var_.value.as_double = d;
  needs_release_ = false;
}

Var::Var(const char* utf8_str) {
  if (has_interface<PPB_Var_Interface_Type>()) {
    uint32_t len = utf8_str ? static_cast<uint32_t>(strlen(utf8_str)) : 0;
    var_ = get_interface<PPB_Var_Interface_Type>()->VarFromUtf8(
        Module::Get()->pp_module(), utf8_str, len);
  } else {
    var_.type = PP_VARTYPE_NULL;
    var_.padding = 0;
  }
  needs_release_ = (var_.type == PP_VARTYPE_STRING);
}

Var::Var(const std::string& utf8_str) {
  if (has_interface<PPB_Var_Interface_Type>()) {
    var_ = get_interface<PPB_Var_Interface_Type>()->VarFromUtf8(
        Module::Get()->pp_module(),
        utf8_str.c_str(),
        static_cast<uint32_t>(utf8_str.size()));
  } else {
    var_.type = PP_VARTYPE_NULL;
    var_.padding = 0;
  }
  needs_release_ = (var_.type == PP_VARTYPE_STRING);
}

Var::Var(const Var& other) {
  var_ = other.var_;
  if (NeedsRefcounting(var_)) {
    if (has_interface<PPB_Var_Interface_Type>()) {
      needs_release_ = true;
      get_interface<PPB_Var_Interface_Type>()->AddRef(var_);
    } else {
      var_.type = PP_VARTYPE_NULL;
      needs_release_ = false;
    }
  } else {
    needs_release_ = false;
  }
}

Var::~Var() {
  if (needs_release_ && has_interface<PPB_Var_Interface_Type>())
    get_interface<PPB_Var_Interface_Type>()->Release(var_);
}

Var& Var::operator=(const Var& other) {
  if (needs_release_ && has_interface<PPB_Var_Interface_Type>())
    get_interface<PPB_Var_Interface_Type>()->Release(var_);
  var_ = other.var_;
  if (NeedsRefcounting(var_)) {
    if (has_interface<PPB_Var_Interface_Type>()) {
      needs_release_ = true;
      get_interface<PPB_Var_Interface_Type>()->AddRef(var_);
    } else {
      var_.type = PP_VARTYPE_NULL;
      needs_release_ = false;
    }
  } else {
    needs_release_ = false;
  }
  return *this;
}

bool Var::operator==(const Var& other) const {
  if (var_.type != other.var_.type)
    return false;
  switch (var_.type) {
    case PP_VARTYPE_UNDEFINED:
    case PP_VARTYPE_NULL:
      return true;
    case PP_VARTYPE_BOOL:
      return AsBool() == other.AsBool();
    case PP_VARTYPE_INT32:
      return AsInt() == other.AsInt();
    case PP_VARTYPE_DOUBLE:
      return AsDouble() == other.AsDouble();
    case PP_VARTYPE_STRING:
      if (var_.value.as_id == other.var_.value.as_id)
        return true;
      return AsString() == other.AsString();
    default:  // Objects, arrays, dictionaries.
      return var_.value.as_id == other.var_.value.as_id;
  }
}

bool Var::AsBool() const {
  if (!is_bool()) {
    PP_NOTREACHED();
    return false;
  }
  return PPBoolToBool(var_.value.as_bool);
}

int32_t Var::AsInt() const {
  if (is_int())
    return var_.value.as_int;
  if (is_double())
    return static_cast<int>(var_.value.as_double);
  PP_NOTREACHED();
  return 0;
}

double Var::AsDouble() const {
  if (is_double())
    return var_.value.as_double;
  if (is_int())
    return static_cast<double>(var_.value.as_int);
  PP_NOTREACHED();
  return 0.0;
}

std::string Var::AsString() const {
  if (!is_string()) {
    PP_NOTREACHED();
    return std::string();
  }

  if (!has_interface<PPB_Var_Interface_Type>())
    return std::string();
  uint32_t len;
  const char* str =
      get_interface<PPB_Var_Interface_Type>()->VarToUtf8(var_, &len);
  return std::string(str, len);
}

#ifndef PPAPI_VAR_REMOVE_SCRIPTING
Var::Var(Instance* instance, ScriptableObject* object) {
  if (has_interface<PPB_Var_Deprecated>()) {
    var_ = get_interface<PPB_Var_Deprecated>()->CreateObject(
        instance->pp_instance(), object->GetClass(), object);
    needs_release_ = true;
  } else {
    var_.type = PP_VARTYPE_NULL;
    var_.padding = 0;
    needs_release_ = false;
  }
}

ScriptableObject* Var::AsScriptableObject() const {
  if (!is_object()) {
    PP_NOTREACHED();
  } else if (has_interface<PPB_Var_Deprecated>()) {
    void* object = NULL;
    if (get_interface<PPB_Var_Deprecated>()->IsInstanceOf(
        var_, ScriptableObject::GetClass(), &object)) {
      return reinterpret_cast<ScriptableObject*>(object);
    }
  }
  return NULL;
}

bool Var::HasProperty(const Var& name, Var* exception) const {
  if (!has_interface<PPB_Var_Deprecated>())
    return false;
  return get_interface<PPB_Var_Deprecated>()->HasProperty(
      var_, name.var_, OutException(exception).get());
}

bool Var::HasMethod(const Var& name, Var* exception) const {
  if (!has_interface<PPB_Var_Deprecated>())
    return false;
  return get_interface<PPB_Var_Deprecated>()->HasMethod(
      var_, name.var_, OutException(exception).get());
}

Var Var::GetProperty(const Var& name, Var* exception) const {
  if (!has_interface<PPB_Var_Deprecated>())
    return Var();
  return Var(PassRef(), get_interface<PPB_Var_Deprecated>()->GetProperty(
      var_, name.var_, OutException(exception).get()));
}

void Var::GetAllPropertyNames(std::vector<Var>* properties,
                              Var* exception) const {
  if (!has_interface<PPB_Var_Deprecated>())
    return;
  PP_Var* props = NULL;
  uint32_t prop_count = 0;
  get_interface<PPB_Var_Deprecated>()->GetAllPropertyNames(
      var_, &prop_count, &props, OutException(exception).get());
  if (!prop_count)
    return;
  properties->resize(prop_count);
  for (uint32_t i = 0; i < prop_count; ++i) {
    Var temp(PassRef(), props[i]);
    (*properties)[i] = temp;
  }
  Module::Get()->core()->MemFree(props);
}

void Var::SetProperty(const Var& name, const Var& value, Var* exception) {
  if (!has_interface<PPB_Var_Deprecated>())
    return;
  get_interface<PPB_Var_Deprecated>()->SetProperty(
      var_, name.var_, value.var_, OutException(exception).get());
}

void Var::RemoveProperty(const Var& name, Var* exception) {
  if (!has_interface<PPB_Var_Deprecated>())
    return;
  get_interface<PPB_Var_Deprecated>()->RemoveProperty(
      var_, name.var_, OutException(exception).get());
}

Var Var::Call(const Var& method_name, uint32_t argc, Var* argv,
              Var* exception) {
  if (!has_interface<PPB_Var_Deprecated>())
    return Var();
  if (argc > 0) {
    std::vector<PP_Var> args;
    args.reserve(argc);
    for (size_t i = 0; i < argc; i++)
      args.push_back(argv[i].var_);
    return Var(PassRef(), get_interface<PPB_Var_Deprecated>()->Call(
        var_, method_name.var_, argc, &args[0], OutException(exception).get()));
  } else {
    // Don't try to get the address of a vector if it's empty.
    return Var(PassRef(), get_interface<PPB_Var_Deprecated>()->Call(
        var_, method_name.var_, 0, NULL, OutException(exception).get()));
  }
}

Var Var::Construct(uint32_t argc, Var* argv, Var* exception) const {
  if (!has_interface<PPB_Var_Deprecated>())
    return Var();
  if (argc > 0) {
    std::vector<PP_Var> args;
    args.reserve(argc);
    for (size_t i = 0; i < argc; i++)
      args.push_back(argv[i].var_);
    return Var(PassRef(), get_interface<PPB_Var_Deprecated>()->Construct(
        var_, argc, &args[0], OutException(exception).get()));
  } else {
    // Don't try to get the address of a vector if it's empty.
    return Var(PassRef(), get_interface<PPB_Var_Deprecated>()->Construct(
        var_, 0, NULL, OutException(exception).get()));
  }
}

Var Var::Call(const Var& method_name, Var* exception) {
  if (!has_interface<PPB_Var_Deprecated>())
    return Var();
  return Var(PassRef(), get_interface<PPB_Var_Deprecated>()->Call(
      var_, method_name.var_, 0, NULL, OutException(exception).get()));
}

Var Var::Call(const Var& method_name, const Var& arg1, Var* exception) {
  if (!has_interface<PPB_Var_Deprecated>())
    return Var();
  PP_Var args[1] = {arg1.var_};
  return Var(PassRef(), get_interface<PPB_Var_Deprecated>()->Call(
      var_, method_name.var_, 1, args, OutException(exception).get()));
}

Var Var::Call(const Var& method_name, const Var& arg1, const Var& arg2,
              Var* exception) {
  if (!has_interface<PPB_Var_Deprecated>())
    return Var();
  PP_Var args[2] = {arg1.var_, arg2.var_};
  return Var(PassRef(), get_interface<PPB_Var_Deprecated>()->Call(
      var_, method_name.var_, 2, args, OutException(exception).get()));
}

Var Var::Call(const Var& method_name, const Var& arg1, const Var& arg2,
              const Var& arg3, Var* exception) {
  if (!has_interface<PPB_Var_Deprecated>())
    return Var();
  PP_Var args[3] = {arg1.var_, arg2.var_, arg3.var_};
  return Var(PassRef(), get_interface<PPB_Var_Deprecated>()->Call(
      var_, method_name.var_, 3, args, OutException(exception).get()));
}

Var Var::Call(const Var& method_name, const Var& arg1, const Var& arg2,
              const Var& arg3, const Var& arg4, Var* exception) {
  if (!has_interface<PPB_Var_Deprecated>())
    return Var();
  PP_Var args[4] = {arg1.var_, arg2.var_, arg3.var_, arg4.var_};
  return Var(PassRef(), get_interface<PPB_Var_Deprecated>()->Call(
      var_, method_name.var_, 4, args, OutException(exception).get()));
}
#endif

std::string Var::DebugString() const {
  char buf[256];
  if (is_undefined()) {
    snprintf(buf, sizeof(buf), "Var<UNDEFINED>");
  } else if (is_null()) {
    snprintf(buf, sizeof(buf), "Var<NULL>");
  } else if (is_bool()) {
    snprintf(buf, sizeof(buf), AsBool() ? "Var<true>" : "Var<false>");
  } else if (is_int()) {
    // Note that the following static_cast is necessary because
    // NativeClient's int32_t is actually "long".
    // TODO(sehr,polina): remove this after newlib is changed.
    snprintf(buf, sizeof(buf), "Var<%d>", static_cast<int>(AsInt()));
  } else if (is_double()) {
    snprintf(buf, sizeof(buf), "Var<%f>", AsDouble());
  } else if (is_string()) {
    char format[] = "Var<'%s'>";
    size_t decoration = sizeof(format) - 2;  // The %s is removed.
    size_t available = sizeof(buf) - decoration;
    std::string str = AsString();
    if (str.length() > available) {
      str.resize(available - 3);  // Reserve space for ellipsis.
      str.append("...");
    }
    snprintf(buf, sizeof(buf), format, str.c_str());
  } else if (is_object()) {
    snprintf(buf, sizeof(buf), "Var<OBJECT>");
  }
  return buf;
}

}  // namespace pp
