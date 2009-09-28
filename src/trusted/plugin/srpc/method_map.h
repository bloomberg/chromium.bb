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
    const int kLength = strlen(types);
    if (kLength > NACL_SRPC_MAX_ARGS + 1)
      return false;
    NaClSrpcArg* args = new(std::nothrow) NaClSrpcArg[kLength];
    if (NULL == args) {
      return false;
    }
    memset(static_cast<void*>(args), 0, kLength * sizeof(NaClSrpcArg));
    for (int i = 0; i < kLength; ++i) {
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
    delete[] vec[0];
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
  std::map<int, MethodInfo*> method_map_;
};


}  // namespace nacl_srpc

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_METHOD_MAP_H
