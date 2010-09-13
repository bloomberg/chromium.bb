// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_OBJ_CAPABILITY_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_OBJ_CAPABILITY_H_

#ifdef __native_client__
#include <stdint.h>
#include <sys/types.h>
#include <sys/nacl_imc_api.h>
#else
#include "native_client/src/trusted/service_runtime/include/sys/nacl_imc_api.h"
#endif  // __native_client__

namespace ppapi_proxy {

class ObjectCapability {
 public:
  ObjectCapability() : object_id_(0), pid_(0) {}

  ObjectCapability(int32_t pid, int64_t object_id)
      : object_id_(object_id), pid_(pid) {
  }

  ObjectCapability(uint32_t size, char* bytes) {
    if (size == static_cast<uint32_t>(sizeof(*this))) {
      *this = *reinterpret_cast<ObjectCapability*>(bytes);
    }
  }

  int64_t pid() const { return pid_; }
  void set_pid(int64_t pid) { pid_ = pid; }

  int64_t object_id() const {
    return object_id_;
  }
  void set_object_id(int64_t object_id) {
    object_id_ = object_id;
  }

  // Copies the specified capability value to this capability.
  ObjectCapability& CopyFrom(const ObjectCapability& capability) {
    pid_ = capability.pid_;
    object_id_ = capability.object_id_;
    return *this;
  }

  char* char_addr() { return reinterpret_cast<char*>(this); }
  uint32_t size() { return static_cast<uint32_t>(sizeof(*this)); }

 private:
  uint64_t object_id_;  // The object_id from a PP_Var in the owner process.
  int64_t pid_;         // The process ID that has the object.
};

// Less (<) is required for the std::map template class.
inline bool operator<(const ObjectCapability& c1, const ObjectCapability& c2) {
  if (c1.pid() == c2.pid())
    return (c1.object_id() < c2.object_id());
  return (c2.pid() < c1.pid());
}

inline bool operator==(const ObjectCapability& c1, const ObjectCapability& c2) {
  return (c1.pid() == c2.pid() && c1.object_id() == c2.object_id());
}

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_OBJ_CAPABILITY_H_
