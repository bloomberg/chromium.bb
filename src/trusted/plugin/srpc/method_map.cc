/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <map>
#include <string>

#include "native_client/src/shared/platform/nacl_log.h"

#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"

#include "native_client/src/trusted/plugin/srpc/method_map.h"
#include "native_client/src/trusted/plugin/srpc/plugin.h"
#include "native_client/src/trusted/plugin/srpc/srpc_client.h"
#include "native_client/src/trusted/plugin/srpc/utility.h"

#ifdef TEXT_COMMAND_LOGGING
#define LOGCOMMAND(fp, args) \
    if (fp) { \
      fprintf args; \
    }
#else
#define LOGCOMMAND(fp, args)
#endif

namespace nacl_srpc {

MethodInfo::~MethodInfo() {
    free(reinterpret_cast<void*>(name_));
    free(reinterpret_cast<void*>(ins_));
    free(reinterpret_cast<void*>(outs_));
  }

char* MethodInfo::TypeName(NaClSrpcArgType type) {
  const char* str;

  str = "BAD_TYPE";
  switch (type) {
    case NACL_SRPC_ARG_TYPE_INVALID:
      str = "INVALID";
      break;
    case NACL_SRPC_ARG_TYPE_DOUBLE:
      str = "double";
      break;
    case NACL_SRPC_ARG_TYPE_INT:
      str = "int";
      break;
    case NACL_SRPC_ARG_TYPE_STRING:
      str = "string";
      break;
    case NACL_SRPC_ARG_TYPE_HANDLE:
      str = "handle";
      break;
    case NACL_SRPC_ARG_TYPE_BOOL:
      str = "bool";
      break;
    case NACL_SRPC_ARG_TYPE_CHAR_ARRAY:
      str = "char[]";
      break;
    case NACL_SRPC_ARG_TYPE_INT_ARRAY:
      str = "int[]";
      break;
    case NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY:
      str = "double[]";
      break;
    case NACL_SRPC_ARG_TYPE_OBJECT:
      str = "object";
      break;
    case NACL_SRPC_ARG_TYPE_VARIANT_ARRAY:
      str = "variant[]";
      break;
    default:
      str = "BAD TYPE";
      break;
  }
  return PortablePluginInterface::MemAllocStrdup(str);
}

bool InitSrpcArgArray(NaClSrpcArg* arr, int size) {
  arr->tag = NACL_SRPC_ARG_TYPE_VARIANT_ARRAY;
  arr->u.vaval.varr = new(std::nothrow) NaClSrpcArg[size];
  if (NULL == arr->u.vaval.varr) {
    arr->u.vaval.count = 0;
    return false;
  }
  arr->u.vaval.count = size;
  return true;
}

void FreeSrpcArg(NaClSrpcArg* arg) {
  switch (arg->tag) {
    case NACL_SRPC_ARG_TYPE_CHAR_ARRAY:
      free(arg->u.caval.carr);
      break;
    case NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY:
      free(arg->u.daval.darr);
      break;
    case NACL_SRPC_ARG_TYPE_HANDLE:
      break;
    case NACL_SRPC_ARG_TYPE_INT_ARRAY:
      free(arg->u.iaval.iarr);
      break;
    case NACL_SRPC_ARG_TYPE_STRING:
      // All strings that are passed in SrpcArg must be allocated using
      // malloc! We cannot use browser's allocation API
      // since some of SRPC arguments is handled outside of the plugin code.
      free(arg->u.sval);
      break;
    case NACL_SRPC_ARG_TYPE_VARIANT_ARRAY:
      if (arg->u.vaval.varr) {
        for (uint32_t i=0; i < arg->u.vaval.count; i++) {
          FreeSrpcArg(&arg->u.vaval.varr[i]);
        }
      }
      break;
    case NACL_SRPC_ARG_TYPE_OBJECT:
      // This is a pointer to a scriptable object and should be released
      // by the browser
      break;
    case NACL_SRPC_ARG_TYPE_BOOL:
    case NACL_SRPC_ARG_TYPE_DOUBLE:
    case NACL_SRPC_ARG_TYPE_INT:
    case NACL_SRPC_ARG_TYPE_INVALID:
    default:
      break;
  }
}

bool MethodInfo::Signature(NaClSrpcArg* toplevel) {
  InitSrpcArgArray(toplevel, 3);

  dprintf(("Signature: %p->[0] = %p (%s)\n",
           static_cast<void *>(toplevel), static_cast<void *>(name_), name_));

  NaClSrpcArg* name_arg = &toplevel->u.vaval.varr[0];
  char *temp_name = PortablePluginInterface::MemAllocStrdup(name_);
  // The ownership of this memory, if this method is successful, is
  // passed to the NaClSrpcArg object *toplevel.  This code is used by
  // GetSignatureObject in ConnectedSocket (via SrpcClient's
  // GetSignatureObject), and the dtor for ConnectedSocket performs
  // the deallocation.

  STRINGZ_TO_SRPCARG(temp_name, *name_arg);

  NaClSrpcArg *args = &toplevel->u.vaval.varr[1];
  uint32_t ins_length = strlen(ins_);

  if (!InitSrpcArgArray(args, ins_length)) {
    free(temp_name);
    return false;
  }

  dprintf(("Signature: %p->[1] = %p\n",
           static_cast<void *>(toplevel),
           static_cast<void *>(args)));

  uint32_t i;
  for (i = 0; i < ins_length; ++i) {
    dprintf(("Signature: %p->[1][%d] = %p (%s)\n",
             static_cast<void *>(&args->u.vaval.varr[i]),
             i,
             static_cast<void *>(
                 TypeName(static_cast<NaClSrpcArgType>(ins_[i]))),
             TypeName(static_cast<NaClSrpcArgType>(ins_[i]))));
    STRINGZ_TO_SRPCARG(TypeName(static_cast<NaClSrpcArgType>(ins_[i])),
                         args->u.vaval.varr[i]);
  }

  NaClSrpcArg *rets = &toplevel->u.vaval.varr[2];
  uint32_t const outs_length = strlen(outs_);

  if (!InitSrpcArgArray(rets, outs_length)) {
    FreeSrpcArg(args);
    free(temp_name);
    return false;
  }

  dprintf(("Signature: %p->[2] = %p\n",
           static_cast<void *>(toplevel),
           static_cast<void *>(rets)));

  for (i = 0; i < outs_length; ++i) {
    dprintf(("Signature: %p->[2][%d] = %p (%s)\n",
             static_cast<void *>(&rets->u.vaval.varr[i]),
             i,
             static_cast<void *>(
                 TypeName(static_cast<NaClSrpcArgType>(outs_[i]))),
             TypeName(static_cast<NaClSrpcArgType>(outs_[i]))));
    STRINGZ_TO_SRPCARG(TypeName(static_cast<NaClSrpcArgType>(outs_[i])),
                         rets->u.vaval.varr[i]);
  }
  return true;
}

void MethodInfo::PrintType(Plugin* plugin, NaClSrpcArgType type) {
  UNREFERENCED_PARAMETER(plugin);
  UNREFERENCED_PARAMETER(type);
}

MethodMap::~MethodMap() {
  std::map<int, MethodInfo*>::iterator it;
  while ((it = method_map_.begin()) != method_map_.end()) {
    delete(it->second);
    method_map_.erase(it);
  }
}

MethodInfo* MethodMap::GetMethod(uintptr_t method_id) {
  return method_map_[method_id];
}

void MethodMap::AddMethod(uintptr_t method_id, MethodInfo *info) {
  if (method_map_.find(method_id) != method_map_.end()) {
    // the method already exists
    abort();
  }
  method_map_[method_id] = info;
}

}  // namespace nacl_srpc
