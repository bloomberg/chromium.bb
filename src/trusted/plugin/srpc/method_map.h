/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_METHOD_MAP_H
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_METHOD_MAP_H

#include "native_client/src/include/portability_string.h"

#include <limits.h>
#include <map>

#include "native_client/src/shared/srpc/nacl_srpc.h"

namespace nacl_srpc {

class Plugin;

bool InitSrpcArgArray(NaClSrpcArg* arr, int size);
void FreeSrpcArg(NaClSrpcArg* arg);


// A utility class that builds and deletes parameter vectors used in
// rpcs.
class SrpcParams {
 public:

  SrpcParams() : exception_info_(NULL) {
    memset(ins, 0, sizeof(ins));
    memset(outs, 0, sizeof(outs));
  }

  SrpcParams(const char* in_types, const char* out_types)
      : exception_info_(NULL) {
    if (!Init(in_types, out_types)) {
      FreeAll();
    }
  }

  ~SrpcParams() {
    FreeAll();
    free(exception_info_);
  }

  bool Init(const char* in_types, const char* out_types) {
    if (!FillVec(ins, in_types)) {
      return false;
    }
    if (!FillVec(outs, out_types)) {
      FreeArguments(ins);
      return false;
    }
    return true;
  }

  NaClSrpcArg* Input(int index) {
    return ins[index];
  }

  NaClSrpcArg* Output(int index) {
    return outs[index];
  }

  void SetExceptionInfo(const char *msg) {
    exception_info_ = STRDUP(msg);
  }

  bool HasExceptionInfo() {
    return (NULL != exception_info_);
  }

  char* GetExceptionInfo() {
    return exception_info_;
  }

 private:
  void FreeAll() {
    FreeArguments(ins);
    FreeArguments(outs);
    memset(ins, 0, sizeof(ins));
    memset(outs, 0, sizeof(outs));
  }

  bool FillVec(NaClSrpcArg* vec[], const char* types) {
    const size_t kLength = strlen(types);
    if (kLength > NACL_SRPC_MAX_ARGS)
      return false;
    // We use malloc/new here rather than new/delete, because the SRPC layer
    // is written in C, and hence will use malloc/free.
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
    vec[kLength] = NULL;
    return true;
  }

  void FreeArguments(NaClSrpcArg* vec[]) {
    if (NULL == vec[0]) {
      return;
    }
    for (NaClSrpcArg** argp = vec; *argp; ++argp) {
      FreeSrpcArg(*argp);
    }
    // Free the vector containing the arguments themselves.
    free(vec[0]);
  }

 public:
  // The ins and outs arrays contain one more element, to hold a NULL pointer
  // to indicate the end of the list.
  NaClSrpcArg* ins[NACL_SRPC_MAX_ARGS + 1];
  NaClSrpcArg* outs[NACL_SRPC_MAX_ARGS + 1];

 private:
  char *exception_info_;
};

typedef bool(*RpcFunction)(void *obj, SrpcParams *params);

// MethodInfo records the method names and type signatures of an SRPC server.
class MethodInfo {
 public:
  // statically defined method - called through a pointer
  MethodInfo(const RpcFunction function_ptr,
             const char* name,
             const char* ins,
             const char* outs,
             // index is set to UINT_MAX for methods implemented by the plugin,
             // All methods implemented by nacl modules have indexes
             // that are lower than UINT_MAX.
             const uint32_t index = UINT_MAX) :
    function_ptr_(function_ptr),
    name_(STRDUP(name)),
    ins_(STRDUP(ins)),
    outs_(STRDUP(outs)),
    index_(index) { }
  ~MethodInfo();

  char* TypeName(NaClSrpcArgType type);
  bool Signature(NaClSrpcArg* toplevel);

 private:
  void PrintType(Plugin* plugin, NaClSrpcArgType type);
 public:
  RpcFunction function_ptr_;
  char* name_;
  char* ins_;
  char* outs_;
  uint32_t index_;
};

class MethodMap {
 public:
  MethodMap() {}
  ~MethodMap();
  MethodInfo* GetMethod(uintptr_t method_id);
  void AddMethod(uintptr_t method_id, MethodInfo *info);

 private:
  typedef std::map<uintptr_t, MethodInfo*> MethodMapStorage;
  MethodMapStorage method_map_;
};


}  // namespace nacl_srpc

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_METHOD_MAP_H
