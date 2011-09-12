/*
 * Copyright 2008 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include <map>

#include "native_client/src/include/checked_cast.h"
#include "native_client/src/shared/platform/nacl_exit.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"
#include "native_client/src/trusted/plugin/method_map.h"
#include "native_client/src/trusted/plugin/plugin.h"
#include "native_client/src/trusted/plugin/srpc_client.h"
#include "native_client/src/trusted/plugin/utility.h"

using nacl::assert_cast;

namespace {

uint32_t ArgsLength(NaClSrpcArg** index) {
  uint32_t i;
  for (i = 0; (i < NACL_SRPC_MAX_ARGS) && NULL != index[i]; ++i) {
    // Empty body.  Avoids warning.
  }
  return i;
}

}  // namespace

namespace plugin {

bool SrpcParams::Init(const char* in_types, const char* out_types) {
  if (!FillVec(ins_, in_types)) {
    return false;
  }
  if (!FillVec(outs_, out_types)) {
    FreeArguments(ins_);
    return false;
  }
  return true;
}

uint32_t SrpcParams::InputLength() const {
  return ArgsLength(const_cast<NaClSrpcArg**>(ins_));
}

uint32_t SrpcParams::OutputLength() const {
  return ArgsLength(const_cast<NaClSrpcArg**>(outs_));
}

uint32_t SrpcParams::SignatureLength() const {
  uint32_t in_length = ArgsLength(const_cast<NaClSrpcArg**>(ins_));
  uint32_t out_length = ArgsLength(const_cast<NaClSrpcArg**>(outs_));
  uint32_t array_outs = 0;

  for (uint32_t i = 0; i < out_length; ++i) {
    switch (outs_[i]->tag) {
      case NACL_SRPC_ARG_TYPE_CHAR_ARRAY:
      case NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY:
      case NACL_SRPC_ARG_TYPE_INT_ARRAY:
      case NACL_SRPC_ARG_TYPE_LONG_ARRAY:
        ++array_outs;
        break;
      case NACL_SRPC_ARG_TYPE_STRING:
      case NACL_SRPC_ARG_TYPE_BOOL:
      case NACL_SRPC_ARG_TYPE_DOUBLE:
      case NACL_SRPC_ARG_TYPE_INT:
      case NACL_SRPC_ARG_TYPE_LONG:
      case NACL_SRPC_ARG_TYPE_HANDLE:
      case NACL_SRPC_ARG_TYPE_INVALID:
      case NACL_SRPC_ARG_TYPE_OBJECT:
      case NACL_SRPC_ARG_TYPE_VARIANT_ARRAY:
      default:
        break;
    }
  }
  return in_length + array_outs;
}

void SrpcParams::FreeAll() {
  FreeArguments(ins_);
  FreeArguments(outs_);
  memset(ins_, 0, sizeof(ins_));
  memset(outs_, 0, sizeof(outs_));
}

bool SrpcParams::FillVec(NaClSrpcArg* vec[], const char* types) {
  const size_t kLength = strlen(types);
  if (kLength > NACL_SRPC_MAX_ARGS) {
    return false;
  }
  // We use malloc/new here rather than new/delete, because the SRPC layer
  // is written in C and hence will use malloc/free.
  // This array will get deallocated by FreeArguments().
  if (kLength > 0) {
    NaClSrpcArg* args =
      reinterpret_cast<NaClSrpcArg*>(malloc(kLength * sizeof(*args)));
    if (NULL == args) {
      return false;
    }

    memset(static_cast<void*>(args), 0, kLength * sizeof(*args));
    for (size_t i = 0; i < kLength; ++i) {
      vec[i] = &args[i];
      args[i].tag = static_cast<NaClSrpcArgType>(types[i]);
    }
  }
  vec[kLength] = NULL;
  return true;
}

void SrpcParams::FreeArguments(NaClSrpcArg* vec[]) {
  if (NULL == vec[0]) {
    return;
  }
  for (NaClSrpcArg** argp = vec; *argp; ++argp) {
    FreeSrpcArg(*argp);
  }
  // Free the vector containing the arguments themselves that was
  // allocated with FillVec().
  free(vec[0]);
}

MethodInfo::~MethodInfo() {
    free(reinterpret_cast<void*>(name_));
    free(reinterpret_cast<void*>(ins_));
    free(reinterpret_cast<void*>(outs_));
  }

bool InitSrpcArgArray(NaClSrpcArg* arr, int size) {
  arr->tag = NACL_SRPC_ARG_TYPE_VARIANT_ARRAY;
  arr->arrays.varr = reinterpret_cast<NaClSrpcArg*>(
      calloc(size, sizeof(*(arr->arrays.varr))));
  if (NULL == arr->arrays.varr) {
    arr->u.count = 0;
    return false;
  }
  arr->u.count = size;
  return true;
}

void FreeSrpcArg(NaClSrpcArg* arg) {
  switch (arg->tag) {
    case NACL_SRPC_ARG_TYPE_CHAR_ARRAY:
      free(arg->arrays.carr);
      break;
    case NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY:
      free(arg->arrays.darr);
      break;
    case NACL_SRPC_ARG_TYPE_HANDLE:
      break;
    case NACL_SRPC_ARG_TYPE_INT_ARRAY:
      free(arg->arrays.iarr);
      break;
    case NACL_SRPC_ARG_TYPE_LONG_ARRAY:
      free(arg->arrays.larr);
      break;
    case NACL_SRPC_ARG_TYPE_STRING:
      // All strings that are passed in SrpcArg must be allocated using
      // malloc! We cannot use browser's allocation API
      // since some of SRPC arguments is handled outside of the plugin code.
      free(arg->arrays.str);
      break;
    case NACL_SRPC_ARG_TYPE_VARIANT_ARRAY:
      if (arg->arrays.varr) {
        for (uint32_t i = 0; i < arg->u.count; i++) {
          FreeSrpcArg(&arg->arrays.varr[i]);
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
    case NACL_SRPC_ARG_TYPE_LONG:
    case NACL_SRPC_ARG_TYPE_INVALID:
    default:
      break;
  }
}

MethodMap::~MethodMap() {
  MethodMapStorage::iterator it;
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
    NaClAbort();
  }
  method_map_[method_id] = info;
  method_map_keys_.push_back(method_id);
}

}  // namespace plugin
