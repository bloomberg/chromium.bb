/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// Lookup table types for method dispatching.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_METHOD_MAP_H
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_METHOD_MAP_H

#include <limits.h>
#include <map>
#include <vector>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability_string.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

namespace plugin {

class Plugin;

bool InitSrpcArgArray(NaClSrpcArg* arr, int size);
void FreeSrpcArg(NaClSrpcArg* arg);

// A utility class that builds and deletes parameter vectors used in rpcs.
class SrpcParams {
 public:
  SrpcParams() : exception_string_(NULL) {
    memset(ins_, 0, sizeof(ins_));
    memset(outs_, 0, sizeof(outs_));
  }

  SrpcParams(const char* in_types, const char* out_types)
      : exception_string_(NULL) {
    if (!Init(in_types, out_types)) {
      FreeAll();
    }
  }

  ~SrpcParams() {
    FreeAll();
    free(exception_string_);
  }

  bool Init(const char* in_types, const char* out_types);
  uint32_t SignatureLength() const;
  uint32_t InputLength() const;
  uint32_t OutputLength() const;

  NaClSrpcArg** ins() const { return const_cast<NaClSrpcArg**>(ins_); }
  NaClSrpcArg** outs() const { return const_cast<NaClSrpcArg**>(outs_); }

  char* exception_string() const { return exception_string_; }
  void set_exception_string(const char* msg) {
    exception_string_ = STRDUP(msg);
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(SrpcParams);
  void FreeAll();
  bool FillVec(NaClSrpcArg* vec[], const char* types);
  void FreeArguments(NaClSrpcArg* vec[]);
  // The ins_ and outs_ arrays contain one more element, to hold a NULL pointer
  // to indicate the end of the list.
  NaClSrpcArg* ins_[NACL_SRPC_MAX_ARGS + 1];
  NaClSrpcArg* outs_[NACL_SRPC_MAX_ARGS + 1];
  char* exception_string_;
};

typedef bool (*RpcFunction)(void* obj, SrpcParams* params);

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

  RpcFunction function_ptr() const { return function_ptr_; }
  char* name() const { return name_; }
  char* ins() const { return ins_; }
  char* outs() const { return outs_; }
  uint32_t index() const { return index_; }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(MethodInfo);
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
  void AddMethod(uintptr_t method_id, MethodInfo* info);

  typedef std::vector<uintptr_t> MethodMapKeys;
  MethodMapKeys* Keys() { return &method_map_keys_; }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(MethodMap);
  typedef std::map<uintptr_t, MethodInfo*> MethodMapStorage;
  MethodMapStorage method_map_;
  MethodMapKeys method_map_keys_;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_METHOD_MAP_H
