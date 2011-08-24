// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_var.h"

#include <stdio.h>

#include <string>
#include <utility>

#include "native_client/src/include/checked_cast.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/ppapi_proxy/proxy_var_cache.h"
#include "native_client/src/shared/ppapi_proxy/string_proxy_var.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_var.h"

namespace ppapi_proxy {

namespace {

void AddRef(PP_Var var) {
  DebugPrintf("PPB_Var::AddRef: var=PPB_Var(%s)\n",
              PluginVar::DebugString(var).c_str());
  ProxyVarCache::GetInstance().RetainProxyVar(var);
}

void Release(PP_Var var) {
  DebugPrintf("PPB_Var::Release: var=PPB_Var(%s)\n",
              PluginVar::DebugString(var).c_str());
  ProxyVarCache::GetInstance().ReleaseProxyVar(var);
}

PP_Var VarFromUtf8(PP_Module module_id, const char* data, uint32_t len) {
  DebugPrintf("PPB_Var::VarFromUtf8: data='%.*s'\n", len, data);
  UNREFERENCED_PARAMETER(module_id);
  if (!StringIsUtf8(data, len)) {
    DebugPrintf("PPB_Var::VarFromUtf8: not UTF8\n");
    return PP_MakeNull();
  }
  SharedProxyVar proxy_var(new StringProxyVar(data, len));
  ProxyVarCache::GetInstance().RetainSharedProxyVar(proxy_var);
  PP_Var var;
  var.type = PP_VARTYPE_STRING;
  var.value.as_id = proxy_var->id();
  // Increment the reference count for the return to the caller.
  AddRef(var);
  DebugPrintf("PPB_Var::VarFromUtf8: as_id=%"NACL_PRId64"\n", var.value.as_id);
  return var;
}

const char* VarToUtf8(PP_Var var, uint32_t* len) {
  DebugPrintf("PPB_Var::VarToUtf8: as_id=%"NACL_PRId64"\n", var.value.as_id);
  SharedStringProxyVar string_var = StringProxyVar::CastFromProxyVar(
      ProxyVarCache::GetInstance().SharedProxyVarForVar(var));
  const char* data = NULL;
  if (string_var == NULL) {
    *len = 0;
  } else {
    *len = static_cast<uint32_t>(string_var->contents().size());
    // Mimics PPAPI implementation: as long as SharedStringProxyVar is alive,
    // the return value can be validly used.
    data = string_var->contents().c_str();
  }
  DebugPrintf("PPB_Var::VarToUtf8: data='%.*s'\n", *len, data);
  return data;
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

}  // namespace

const PPB_Var* PluginVar::GetInterface() {
  static const PPB_Var var_interface = {
    AddRef,
    Release,
    VarFromUtf8,
    VarToUtf8
  };
  return &var_interface;
}

std::string PluginVar::DebugString(const PP_Var& var) {
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
        SNPRINTF(buf, kBufSize, "%"NACL_PRIu64"", GetVarId(var));
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

PP_Var PluginVar::StringToPPVar(PP_Module module_id, const std::string& str) {
  static const PPB_Var* ppb_var = NULL;
  if (ppb_var == NULL) {
    ppb_var = static_cast<const PPB_Var*>(
        ppapi_proxy::PluginVar::GetInterface());
  }
  if (ppb_var == NULL) {
    return PP_MakeUndefined();
  }
  return ppb_var->VarFromUtf8(
      module_id, str.c_str(), nacl::assert_cast<uint32_t>(str.size()));
}

std::string PluginVar::PPVarToString(const PP_Var& var) {
  static const PPB_Var* ppb_var = NULL;
  if (ppb_var == NULL) {
    ppb_var = static_cast<const PPB_Var*>(
        ppapi_proxy::PluginVar::GetInterface());
  }
  if (ppb_var == NULL || var.type != PP_VARTYPE_STRING) {
    return "";
  }
  uint32_t len;
  return ppb_var->VarToUtf8(var, &len);
}

void PluginVar::Print(const PP_Var& var) {
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
      DebugPrintf("PP_Var(object: %"NACL_PRIu64")", GetVarId(var));
      break;
    case PP_VARTYPE_ARRAY:
    case PP_VARTYPE_DICTIONARY:
      NACL_NOTREACHED();
      break;
  }
}

}  // namespace ppapi_proxy
