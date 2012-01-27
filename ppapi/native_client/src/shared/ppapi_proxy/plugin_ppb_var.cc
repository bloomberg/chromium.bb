// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "native_client/src/shared/ppapi_proxy/array_buffer_proxy_var.h"
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

PP_Var VarFromUtf8(const char* data, uint32_t len) {
  DebugPrintf("PPB_Var::VarFromUtf8: data='%.*s'\n", len, data);
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

PP_Var VarFromUtf8_1_0(PP_Module /*module*/, const char* data, uint32_t len) {
  return VarFromUtf8(data, len);
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

PP_Var CreateArrayBuffer(uint32_t size_in_bytes) {
  DebugPrintf("PPB_VarArrayBuffer::Create: size_in_bytes=%"NACL_PRIu32"\n",
              size_in_bytes);
  SharedProxyVar proxy_var(new ArrayBufferProxyVar(size_in_bytes));
  ProxyVarCache::GetInstance().RetainSharedProxyVar(proxy_var);
  PP_Var var;
  var.type = PP_VARTYPE_ARRAY_BUFFER;
  var.value.as_id = proxy_var->id();
  // Increment the reference count for the return to the caller.
  AddRef(var);
  DebugPrintf("PPB_VarArrayBuffer::Create: as_id=%"NACL_PRId64"\n",
              var.value.as_id);
  return var;
}

PP_Bool ByteLength(PP_Var var, uint32_t* byte_length) {
  DebugPrintf("PPB_VarArrayBuffer::ByteLength: var=PPB_Var(%s)\n",
              PluginVar::DebugString(var).c_str());
  SharedArrayBufferProxyVar buffer_var = ArrayBufferProxyVar::CastFromProxyVar(
      ProxyVarCache::GetInstance().SharedProxyVarForVar(var));
  if (buffer_var) {
    *byte_length = buffer_var->buffer_length();
    DebugPrintf("PPB_VarArrayBuffer::ByteLength: length=%"NACL_PRIu32"\n",
                *byte_length);
    return PP_TRUE;
  }
  return PP_FALSE;
}

void* Map(PP_Var var) {
  DebugPrintf("PPB_VarArrayBuffer::Map: var=PPB_Var(%s)\n",
              PluginVar::DebugString(var).c_str());
  SharedArrayBufferProxyVar buffer_var = ArrayBufferProxyVar::CastFromProxyVar(
      ProxyVarCache::GetInstance().SharedProxyVarForVar(var));
  if (!buffer_var)
    return NULL;
  void* data = buffer_var->buffer();
  DebugPrintf("PPB_VarArrayBuffer::Map: buffer=%p\n", data);
  return data;
}

void Unmap(PP_Var var) {
  DebugPrintf("PPB_VarArrayBuffer::Unap: var=PPB_Var(%s)\n",
              PluginVar::DebugString(var).c_str());
  // We don't use shared memory, so there's nothing to do.
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

const PPB_Var_1_0* PluginVar::GetInterface1_0() {
  static const PPB_Var_1_0 var_interface = {
    AddRef,
    Release,
    VarFromUtf8_1_0,
    VarToUtf8
  };
  return &var_interface;
}

const PPB_VarArrayBuffer* PluginVar::GetArrayBufferInterface() {
  static const PPB_VarArrayBuffer interface = {
    CreateArrayBuffer,
    ByteLength,
    Map,
    Unmap
  };
  return &interface;
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
        SNPRINTF(buf, kBufSize, "%"NACL_PRId64"", GetVarId(var));
        return std::string("##OBJECT##") + buf + "##";
      }
    case PP_VARTYPE_ARRAY_BUFFER:
      {
        char buf[32];
        const size_t kBufSize = sizeof(buf);
        SNPRINTF(buf, kBufSize, "%"NACL_PRId64"", GetVarId(var));
        return std::string("##ARRAYBUFFER##") + buf + "##";
      }
    case PP_VARTYPE_ARRAY:
    case PP_VARTYPE_DICTIONARY:
      NACL_NOTREACHED();
      break;
  }
  ASSERT_MSG(0, "Unexpected type seen");
  return "##ERROR##";
}

PP_Var PluginVar::StringToPPVar(const std::string& str) {
  static const PPB_Var* ppb_var = NULL;
  if (ppb_var == NULL) {
    ppb_var = static_cast<const PPB_Var*>(
        ppapi_proxy::PluginVar::GetInterface());
  }
  if (ppb_var == NULL) {
    return PP_MakeUndefined();
  }
  return ppb_var->VarFromUtf8(str.c_str(),
                              nacl::assert_cast<uint32_t>(str.size()));
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
    case PP_VARTYPE_ARRAY_BUFFER:
      DebugPrintf("PP_Var(object: %"NACL_PRIu64")", GetVarId(var));
      break;
    case PP_VARTYPE_ARRAY:
    case PP_VARTYPE_DICTIONARY:
      break;
  }
}

}  // namespace ppapi_proxy
