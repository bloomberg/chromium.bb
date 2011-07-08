// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_var_deprecated.h"

#include <stdio.h>

#include <map>
#include <string>
#include <utility>

#include "native_client/src/include/checked_cast.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/ppapi_proxy/object_proxy_var.h"
#include "native_client/src/shared/ppapi_proxy/proxy_var_cache.h"
#include "native_client/src/shared/ppapi_proxy/string_proxy_var.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/third_party/ppapi/c/pp_var.h"

namespace ppapi_proxy {

namespace {
void AddRef(PP_Var var) {
  DebugPrintf("PPB_Var_Deprecated::AddRef(%"NACL_PRId64")\n", var.value.as_id);
  ProxyVarCache::GetInstance().RetainProxyVar(var);
}

void Release(PP_Var var) {
  DebugPrintf("PPB_Var_Deprecated::Release(%"NACL_PRId64")\n", var.value.as_id);
  ProxyVarCache::GetInstance().ReleaseProxyVar(var);
}

PP_Var VarFromUtf8(PP_Module module_id, const char* data, uint32_t len) {
  UNREFERENCED_PARAMETER(module_id);
  if (!StringIsUtf8(data, len)) {
    DebugPrintf("PPB_Var_Deprecated::VarFromUtf8: string '%.*s' is not UTF8\n",
                len, data);
    return PP_MakeNull();
  }
  SharedProxyVar proxy_var(new StringProxyVar(data, len));
  ProxyVarCache::GetInstance().RetainSharedProxyVar(proxy_var);
  PP_Var result;
  result.type = PP_VARTYPE_STRING;
  result.value.as_id = proxy_var->id();
  return result;
}

const char* VarToUtf8(PP_Var var, uint32_t* len) {
  SharedStringProxyVar string_var = StringProxyVar::CastFromProxyVar(
      ProxyVarCache::GetInstance().SharedProxyVarForVar(var));
  if (string_var == NULL) {
    *len = 0;
    return NULL;
  } else {
    *len = static_cast<uint32_t>(string_var->contents().size());
    // Mimics PPAPI implementation: as long as StrImpl is alive, the
    // return value can be validly used.
    return string_var->contents().c_str();
  }
}

bool HasProperty(PP_Var object,
                 PP_Var name,
                 PP_Var* exception) {
  SharedObjectProxyVar object_var = ObjectProxyVar::CastFromProxyVar(
      ProxyVarCache::GetInstance().SharedProxyVarForVar(object));
  if (object_var == NULL) {
    if (exception != NULL) {
      *exception =
          PluginVarDeprecated::StringToPPVar(
              0, "HasProperty called on non-object");
    }
    return false;
  }
  DebugPrintf("PPB_Var_Deprecated::HasProperty: id=%"NACL_PRId64"\n",
              object_var->id());
  DebugPrintf("PPB_Var_Deprecated::HasProperty: "
              "object.type = %d; name.type = %d\n", object.type, name.type);
  const PPP_Class_Deprecated* object_class = object_var->contents().first;
  if (object_class == NULL || object_class->HasProperty == NULL) {
    return false;
  }
  return object_class->HasProperty(object_var->contents().second,
                                   name,
                                   exception);
}

bool HasMethod(PP_Var object,
               PP_Var name,
               PP_Var* exception) {
  SharedObjectProxyVar object_var = ObjectProxyVar::CastFromProxyVar(
      ProxyVarCache::GetInstance().SharedProxyVarForVar(object));
  if (object_var == NULL) {
    if (exception != NULL) {
      *exception =
          PluginVarDeprecated::StringToPPVar(
              0, "HasMethod called on non-object");
    }
    return false;
  }
  DebugPrintf("PPB_Var_Deprecated::HasMethod: id=%"NACL_PRId64"\n",
              object_var->id());
  const PPP_Class_Deprecated* object_class = object_var->contents().first;
  if (object_class == NULL || object_class->HasMethod == NULL) {
    return false;
  }
  return object_class->HasMethod(object_var->contents().second,
                                 name,
                                 exception);
}

PP_Var GetProperty(PP_Var object,
                   PP_Var name,
                   PP_Var* exception) {
  SharedObjectProxyVar object_var = ObjectProxyVar::CastFromProxyVar(
      ProxyVarCache::GetInstance().SharedProxyVarForVar(object));
  if (object_var == NULL) {
    if (exception != NULL) {
      *exception =
          PluginVarDeprecated::StringToPPVar(
              0, "GetProperty called on non-object");
    }
    return PP_MakeUndefined();
  }
  DebugPrintf("PPB_Var_Deprecated::GetProperty: id=%"NACL_PRId64"\n",
              object_var->id());
  const PPP_Class_Deprecated* object_class = object_var->contents().first;
  if (object_class == NULL || object_class->GetProperty == NULL) {
    return PP_MakeUndefined();
  }
  return object_class->GetProperty(object_var->contents().second,
                                   name,
                                   exception);
}

void GetAllPropertyNames(PP_Var object,
                         uint32_t* property_count,
                         PP_Var** properties,
                         PP_Var* exception) {
  SharedObjectProxyVar object_var = ObjectProxyVar::CastFromProxyVar(
      ProxyVarCache::GetInstance().SharedProxyVarForVar(object));
  if (object_var == NULL) {
    if (exception != NULL) {
      *exception =
          PluginVarDeprecated::StringToPPVar(0,
              "GetAllPropertyNames called on non-object");
    }
    *property_count = 0;
    *properties = NULL;
    return;
  }
  DebugPrintf("PPB_Var_Deprecated::GetAllPropertyNames: id=%"NACL_PRId64"\n",
              object_var->id());
  const PPP_Class_Deprecated* object_class = object_var->contents().first;
  if (object_class == NULL || object_class->GetAllPropertyNames == NULL) {
    return;
  }
  object_class->GetAllPropertyNames(object_var->contents().second,
                                    property_count,
                                    properties,
                                    exception);
}

void SetProperty(PP_Var object,
                 PP_Var name,
                 PP_Var value,
                 PP_Var* exception) {
  SharedObjectProxyVar object_var = ObjectProxyVar::CastFromProxyVar(
      ProxyVarCache::GetInstance().SharedProxyVarForVar(object));
  if (object_var == NULL) {
    if (exception != NULL) {
      *exception =
          PluginVarDeprecated::StringToPPVar(
              0, "SetProperty called on non-object");
    }
    return;
  }
  DebugPrintf("PPB_Var_Deprecated::SetProperty: id=%"NACL_PRId64"\n",
              object_var->id());
  const PPP_Class_Deprecated* object_class = object_var->contents().first;
  if (object_class == NULL || object_class->SetProperty == NULL) {
    return;
  }
  object_class->SetProperty(object_var->contents().second,
                            name,
                            value,
                            exception);
}

void RemoveProperty(PP_Var object,
                    PP_Var name,
                    PP_Var* exception) {
  SharedObjectProxyVar object_var = ObjectProxyVar::CastFromProxyVar(
      ProxyVarCache::GetInstance().SharedProxyVarForVar(object));
  if (object_var == NULL) {
    if (exception != NULL) {
      *exception =
          PluginVarDeprecated::StringToPPVar(
              0, "RemoveProperty called on non-object");
    }
    return;
  }
  DebugPrintf("PPB_Var_Deprecated::RemoveProperty: id=%"NACL_PRId64"\n",
              object_var->id());
  const PPP_Class_Deprecated* object_class = object_var->contents().first;
  if (object_class == NULL || object_class->RemoveProperty == NULL) {
    return;
  }
  object_class->RemoveProperty(object_var->contents().second, name, exception);
}

PP_Var Call(PP_Var object,
            PP_Var method_name,
            uint32_t argc,
            PP_Var* argv,
            PP_Var* exception) {
  SharedObjectProxyVar object_var = ObjectProxyVar::CastFromProxyVar(
      ProxyVarCache::GetInstance().SharedProxyVarForVar(object));
  if (object_var == NULL) {
    if (exception != NULL) {
      *exception = PluginVarDeprecated::StringToPPVar(
          0, "Call called on non-object");
    }
    return PP_MakeUndefined();
  }
  DebugPrintf("PPB_Var_Deprecated::Call: id=%"NACL_PRId64"\n",
              object_var->id());
  const PPP_Class_Deprecated* object_class = object_var->contents().first;
  if (object_class == NULL || object_class->Call == NULL) {
    return PP_MakeUndefined();
  }
  return object_class->Call(object_var->contents().second,
                            method_name,
                            argc,
                            argv,
                            exception);
}

PP_Var Construct(PP_Var object,
                 uint32_t argc,
                 PP_Var* argv,
                 PP_Var* exception) {
  SharedObjectProxyVar object_var = ObjectProxyVar::CastFromProxyVar(
      ProxyVarCache::GetInstance().SharedProxyVarForVar(object));
  if (object_var == NULL) {
    if (exception != NULL) {
      *exception =
          PluginVarDeprecated::StringToPPVar(
              0, "Construct called on non-object");
    }
    return PP_MakeUndefined();
  }
  DebugPrintf("PPB_Var_Deprecated::Construct: %"NACL_PRId64"\n",
              object_var->id());
  const PPP_Class_Deprecated* object_class = object_var->contents().first;
  if (object_class == NULL || object_class->Construct == NULL) {
    return PP_MakeUndefined();
  }
  return object_class->Construct(object_var->contents().second,
                                 argc,
                                 argv,
                                 exception);
}

bool IsInstanceOf(PP_Var var,
                  const PPP_Class_Deprecated* object_class,
                  void** object_data) {
  SharedObjectProxyVar object_var = ObjectProxyVar::CastFromProxyVar(
      ProxyVarCache::GetInstance().SharedProxyVarForVar(var));
  *object_data = NULL;
  if (object_var == NULL) {
    return false;
  }
  DebugPrintf("PPB_Var_Deprecated::IsInstanceOf: id=%"NACL_PRId64
              "is instance %p %p\n",
              object_var->id(),
              reinterpret_cast<const void*>(object_var->contents().first),
              reinterpret_cast<const void*>(object_class));
  if (object_class != object_var->contents().first) {
    return false;
  }
  *object_data = object_var->contents().second;
  return true;
}

int64_t GetVarId(PP_Var var) {
  SharedProxyVar proxy_var =
      ProxyVarCache::GetInstance().SharedProxyVarForVar(var);
  if (proxy_var == NULL) {
    return -1;
  } else {
    return proxy_var->id();
  }
}

PP_Var CreateObject(PP_Instance instance_id,
                    const PPP_Class_Deprecated* object_class,
                    void* object_data) {
  UNREFERENCED_PARAMETER(instance_id);
  SharedProxyVar proxy_var(
      new ObjectProxyVar(
          ObjectProxyVar::ObjectPair(object_class, object_data)));
  ProxyVarCache::GetInstance().RetainSharedProxyVar(proxy_var);
  DebugPrintf("PPB_Var_Deprecated::CreateObject: "
              "instance_id=%"NACL_PRId32" proxy_var->id=%"NACL_PRId64"\n",
              instance_id,
              proxy_var->id());
  PP_Var result;
  result.type = PP_VARTYPE_OBJECT;
  result.value.as_id = proxy_var->id();
  return result;
}

PP_Var CreateObjectWithModuleDeprecated(
    PP_Module module_id,
    const PPP_Class_Deprecated* object_class,
    void* object_data) {
  UNREFERENCED_PARAMETER(module_id);
  SharedProxyVar proxy_var(
      new ObjectProxyVar(
          ObjectProxyVar::ObjectPair(object_class, object_data)));
  ProxyVarCache::GetInstance().RetainSharedProxyVar(proxy_var);
  DebugPrintf("PPB_Var_Deprecated::CreateObjectWithModuleDeprecated: "
              "module_id=%"NACL_PRId32" cached var id=%"NACL_PRId64"\n",
              module_id,
              proxy_var->id());
  PP_Var result;
  result.type = PP_VARTYPE_OBJECT;
  result.value.as_id = proxy_var->id();
  return result;
}

}  // namespace

const PPB_Var_Deprecated* PluginVarDeprecated::GetInterface() {
  static const PPB_Var_Deprecated var_interface = {
    AddRef,
    Release,
    VarFromUtf8,
    VarToUtf8,
    HasProperty,
    HasMethod,
    GetProperty,
    GetAllPropertyNames,
    SetProperty,
    RemoveProperty,
    Call,
    Construct,
    IsInstanceOf,
    CreateObject,
    CreateObjectWithModuleDeprecated
  };
  return &var_interface;
}

std::string PluginVarDeprecated::DebugString(PP_Var var) {
  switch (var.type) {
    case PP_VARTYPE_UNDEFINED:
      return "##UNDEFINED##";
    case PP_VARTYPE_NULL:
      return "##NULL##";
    case PP_VARTYPE_BOOL:
      return (var.value.as_bool ? "true" : "false");
    case PP_VARTYPE_INT32:
      {
        char buf[32];
        const size_t kBufSize = sizeof(buf);
        SNPRINTF(buf, kBufSize, "%d", static_cast<int>(var.value.as_int));
        return buf;
      }
    case PP_VARTYPE_DOUBLE:
      {
        char buf[32];
        const size_t kBufSize = sizeof(buf);
        SNPRINTF(buf, kBufSize, "%f", var.value.as_double);
        return buf;
      }
    case PP_VARTYPE_STRING:
      {
        uint32_t len;
        const char* data = VarToUtf8(var, &len);
        return std::string(data, len);
      }
    case PP_VARTYPE_OBJECT:
      {
        char buf[32];
        const size_t kBufSize = sizeof(buf);
        SNPRINTF(buf, kBufSize, "%"NACL_PRId64"", GetVarId(var));
        return std::string("##OBJECT##") + buf + "##";
      }
    case PP_VARTYPE_ARRAY:
    case PP_VARTYPE_DICTIONARY:
      NACL_NOTREACHED();
      break;
  }
  ASSERT_MSG(0, "Unexpected type seen");
  return "##ERROR##";
}

PP_Var PluginVarDeprecated::StringToPPVar(PP_Module module_id,
                                          std::string str) {
  static const PPB_Var_Deprecated* ppb_var_deprecated = NULL;
  if (ppb_var_deprecated == NULL) {
    ppb_var_deprecated = reinterpret_cast<const PPB_Var_Deprecated*>(
        ppapi_proxy::PluginVarDeprecated::GetInterface());
  }
  if (ppb_var_deprecated == NULL) {
    return PP_MakeUndefined();
  }
  return ppb_var_deprecated->VarFromUtf8(
      module_id, str.c_str(), nacl::assert_cast<uint32_t>(str.size()));
}

std::string PluginVarDeprecated::PPVarToString(PP_Var var) {
  static const PPB_Var_Deprecated* ppb_var_deprecated = NULL;
  if (ppb_var_deprecated == NULL) {
    ppb_var_deprecated = reinterpret_cast<const PPB_Var_Deprecated*>(
        ppapi_proxy::PluginVarDeprecated::GetInterface());
  }
  if (ppb_var_deprecated == NULL || var.type != PP_VARTYPE_STRING) {
    return "";
  }
  uint32_t len;
  return ppb_var_deprecated->VarToUtf8(var, &len);
}

void PluginVarDeprecated::Print(PP_Var var) {
  switch (var.type) {
    case PP_VARTYPE_UNDEFINED:
      DebugPrintf("PP_Var(undefined)");
      break;
    case PP_VARTYPE_NULL:
      DebugPrintf("PP_Var(null)");
      break;
    case PP_VARTYPE_BOOL:
      DebugPrintf("PP_Var(bool: %s)", var.value.as_bool ? "true" : "false");
      break;
    case PP_VARTYPE_INT32:
      DebugPrintf("PP_Var(int32: %"NACL_PRId32")", var.value.as_int);
      break;
    case PP_VARTYPE_DOUBLE:
      DebugPrintf("PP_Var(double: %f)", var.value.as_double);
      break;
    case PP_VARTYPE_STRING:
      {
        std::string str = DebugString(var);
        DebugPrintf("PP_Var(string: '%*s')",
                    static_cast<uint32_t>(str.size()),
                    str.c_str());
      }
    case PP_VARTYPE_OBJECT:
      DebugPrintf("PP_Var(object: %"NACL_PRId64")", GetVarId(var));
      break;
    case PP_VARTYPE_ARRAY:
    case PP_VARTYPE_DICTIONARY:
      NACL_NOTREACHED();
      break;
  }
}

}  // namespace ppapi_proxy
