// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Scriptable plugin implementation.

#include "native_client/src/trusted/plugin/scriptable_plugin.h"

#include <string.h>

#include <sstream>
#include <string>
#include <vector>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/plugin/plugin.h"
#include "native_client/src/trusted/plugin/utility.h"


namespace plugin {

namespace {

pp::Var Error(const nacl::string& call_name, const char* caller,
              const char* error, pp::Var* exception) {
  nacl::stringstream error_stream;
  error_stream << call_name << ": " << error;
  if (!exception->is_undefined()) {
    error_stream << " - " + exception->AsString();
  }
  // Get the error string in 2 steps; otherwise, the temporary string returned
  // by the stream is destructed, causing a dangling pointer.
  std::string str = error_stream.str();
  const char* e = str.c_str();
  PLUGIN_PRINTF(("ScriptablePlugin::%s (%s)\n", caller, e));
  *exception = pp::Var(e);
  return pp::Var();
}

// In JavaScript, foo[1] is equivalent to foo["1"], so map both indexed and
// string names to a string.
nacl::string NameAsString(const pp::Var& name) {
  if (name.is_string())
    return name.AsString();
  CHECK(name.is_int());
  nacl::stringstream namestream;
  namestream << name.AsInt();
  return namestream.str();
}

// Returns a pp::Var corresponding to |arg| or void. Sets |exception| on error.
pp::Var NaClSrpcArgToPPVar(const NaClSrpcArg* arg, pp::Var* exception) {
  PLUGIN_PRINTF(("  NaClSrpcArgToPPVar (arg->tag='%c')\n", arg->tag));
  pp::Var var;
  switch (arg->tag) {
    case NACL_SRPC_ARG_TYPE_BOOL:
      var = pp::Var(arg->u.bval != 0);
      break;
    case NACL_SRPC_ARG_TYPE_DOUBLE:
      var = pp::Var(arg->u.dval);
      break;
    case NACL_SRPC_ARG_TYPE_INT:
      var = pp::Var(arg->u.ival);
      break;
    case NACL_SRPC_ARG_TYPE_LONG:
      // PPAPI does not have a 64-bit integral type.  Downcast.
      var = pp::Var(static_cast<int32_t>(arg->u.lval));
      break;
    case NACL_SRPC_ARG_TYPE_STRING:
      var = pp::Var(arg->arrays.str);
      break;
    case NACL_SRPC_ARG_TYPE_CHAR_ARRAY:
    case NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY:
    case NACL_SRPC_ARG_TYPE_INT_ARRAY:
    case NACL_SRPC_ARG_TYPE_LONG_ARRAY:
    case NACL_SRPC_ARG_TYPE_OBJECT:
    case NACL_SRPC_ARG_TYPE_HANDLE:
    case NACL_SRPC_ARG_TYPE_VARIANT_ARRAY:
    case NACL_SRPC_ARG_TYPE_INVALID:
    default:
      *exception = "variant array and invalid argument types are not supported";
  }
  PLUGIN_PRINTF(("  NaClSrpcArgToPPVar (return var=%s, exception=%s)\n",
                 var.DebugString().c_str(), exception->DebugString().c_str()));
  return var;
}

}  // namespace

ScriptablePlugin::ScriptablePlugin(Plugin* plugin)
  : var_(NULL), num_unref_calls_(0), plugin_(plugin) {
  PLUGIN_PRINTF(("ScriptablePlugin::ScriptablePlugin (this=%p, plugin=%p)\n",
                 static_cast<void*>(this),
                 static_cast<void*>(plugin)));
}

ScriptablePlugin::~ScriptablePlugin() {
  PLUGIN_PRINTF(("ScriptablePlugin::~ScriptablePlugin (this=%p)\n",
                 static_cast<void*>(this)));
  PLUGIN_PRINTF(("ScriptablePlugin::~ScriptablePlugin (this=%p, return)\n",
                  static_cast<void*>(this)));
}

void ScriptablePlugin::Unref(ScriptablePlugin** handle) {
  if (*handle != NULL) {
    (*handle)->Unref();
    *handle = NULL;
  }
}

ScriptablePlugin* ScriptablePlugin::NewPlugin(Plugin* plugin) {
  PLUGIN_PRINTF(("ScriptablePlugin::NewPlugin (plugin=%p)\n",
                 static_cast<void*>(plugin)));
  if (plugin == NULL) {
    return NULL;
  }
  ScriptablePlugin* scriptable_plugin = new ScriptablePlugin(plugin);
  if (scriptable_plugin == NULL) {
    return NULL;
  }
  PLUGIN_PRINTF(("ScriptablePlugin::NewPlugin (return %p)\n",
                 static_cast<void*>(scriptable_plugin)));
  return scriptable_plugin;
}

bool ScriptablePlugin::HasProperty(const pp::Var& name, pp::Var* exception) {
  UNREFERENCED_PARAMETER(exception);
  PLUGIN_PRINTF(("ScriptablePlugin::HasProperty (this=%p, name=%s)\n",
                 static_cast<void*>(this), name.DebugString().c_str()));
  if (plugin_ == NULL) {
    return false;
  }
  if (!name.is_string() && !name.is_int())
    return false;
  bool has_property = plugin_->HasProperty(name.AsString());
  PLUGIN_PRINTF(("ScriptablePlugin::HasProperty (has_property=%d)\n",
                 has_property));
  return has_property;
}


bool ScriptablePlugin::HasMethod(const pp::Var& name, pp::Var* exception) {
  UNREFERENCED_PARAMETER(exception);
  PLUGIN_PRINTF(("ScriptablePlugin::HasMethod (this=%p, name='%s')\n",
                 static_cast<void*>(this), name.DebugString().c_str()));
  return false;
}


pp::Var ScriptablePlugin::GetProperty(const pp::Var& name,
                                      pp::Var* exception) {
  PLUGIN_PRINTF(("ScriptablePlugin::GetProperty (name=%s)\n",
                 name.DebugString().c_str()));
  if (plugin_ == NULL) {
    return pp::Var();
  }
  // Get the property.
  NaClSrpcArg prop_value;
  nacl::string prop_name = NameAsString(name);
  if (!plugin_->GetProperty(prop_name, &prop_value)) {
    return Error(prop_name, "GetProperty", "invocation failed", exception);
  }
  PLUGIN_PRINTF(("ScriptablePlugin::GetProperty (invocation done)\n"));
  // Marshall output parameter.
  pp::Var property = NaClSrpcArgToPPVar(&prop_value, exception);
  if (!exception->is_undefined()) {
    return Error(prop_name, "GetProperty", "output marshalling failed",
                 exception);
  }
  PLUGIN_PRINTF(("ScriptablePlugin::GetProperty (property=%s)\n",
                 property.DebugString().c_str()));
  return property;
}


void ScriptablePlugin::SetProperty(const pp::Var& name,
                                   const pp::Var& value,
                                   pp::Var* exception) {
  PLUGIN_PRINTF(("ScriptablePlugin::SetProperty (name=%s, value=%s)\n",
                 name.DebugString().c_str(), value.DebugString().c_str()));
  Error("SetProperty", name.DebugString().c_str(),
        "property setting is not supported", exception);
}


void ScriptablePlugin::RemoveProperty(const pp::Var& name,
                                      pp::Var* exception) {
  PLUGIN_PRINTF(("ScriptablePlugin::RemoveProperty (name=%s)\n",
                 name.DebugString().c_str()));
  Error(NameAsString(name), "RemoveProperty",
        "property removal is not supported", exception);
}

void ScriptablePlugin::GetAllPropertyNames(std::vector<pp::Var>* properties,
                                           pp::Var* exception) {
  PLUGIN_PRINTF(("ScriptablePlugin::GetAllPropertyNames ()\n"));
  UNREFERENCED_PARAMETER(properties);
  UNREFERENCED_PARAMETER(exception);
  Error("GetAllPropertyNames", "", "GetAllPropertyNames is not supported",
        exception);
}


pp::Var ScriptablePlugin::Call(const pp::Var& name,
                               const std::vector<pp::Var>& args,
                               pp::Var* exception) {
  PLUGIN_PRINTF(("ScriptablePlugin::Call (name=%s, %"NACL_PRIuS
                 " args)\n", name.DebugString().c_str(), args.size()));
  return Error("Call", name.DebugString().c_str(),
               "method invocation is not supported", exception);
}


pp::Var ScriptablePlugin::Construct(const std::vector<pp::Var>& args,
                                    pp::Var* exception) {
  PLUGIN_PRINTF(("ScriptablePlugin::Construct (%"NACL_PRIuS
                 " args)\n", args.size()));
  return Error("constructor", "Construct", "constructor is not supported",
               exception);
}


ScriptablePlugin* ScriptablePlugin::AddRef() {
  // This is called when we are about to share this object with the browser,
  // and we need to make sure we have an internal plugin reference, so this
  // object doesn't get deallocated when the browser discards its references.
  if (var_ == NULL) {
    var_ = new pp::VarPrivate(plugin_, this);
    CHECK(var_ != NULL);
  }
  PLUGIN_PRINTF(("ScriptablePlugin::AddRef (this=%p, var=%p)\n",
                 static_cast<void*>(this), static_cast<void*>(var_)));
  return this;
}


void ScriptablePlugin::Unref() {
  // We should have no more than one internal owner of this object, so this
  // should be called no more than once.
  CHECK(++num_unref_calls_ == 1);
  PLUGIN_PRINTF(("ScriptablePlugin::Unref (this=%p, var=%p)\n",
                 static_cast<void*>(this), static_cast<void*>(var_)));
  if (var_ != NULL) {
    // We have shared this with the browser while keeping our own var
    // reference, but we no longer need ours. If the browser has copies,
    // it will clean things up later, otherwise this object will get
    // deallocated right away.
    PLUGIN_PRINTF(("ScriptablePlugin::Unref (delete var)\n"));
    pp::Var* var = var_;
    var_ = NULL;
    delete var;
  } else {
    // Neither the browser nor plugin ever var referenced this object,
    // so it can safely discarded.
    PLUGIN_PRINTF(("ScriptablePlugin::Unref (delete this)\n"));
    CHECK(var_ == NULL);
    delete this;
  }
}


}  // namespace plugin
