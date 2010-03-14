// Copyright (c) 2008 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NaCl-NPAPI Interface

#ifndef NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPCAPABILITY_H_
#define NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPCAPABILITY_H_

#ifdef __native_client__
#include <stdint.h>
#include <sys/types.h>
#include <sys/nacl_imc_api.h>
#else
#include "native_client/src/trusted/service_runtime/include/sys/nacl_imc_api.h"
#endif  // __native_client__

namespace nacl {

class NPCapability {
 public:
  NPCapability() :
    object_(0), pid_(0) {
  }

  NPCapability(int32_t pid, NPObject* object) :
    object_(reinterpret_cast<uint64_t>(object)), pid_(pid) {
  }

  NPCapability(nacl_abi_size_t size, char* bytes) {
    if (size == static_cast<nacl_abi_size_t>(sizeof(*this))) {
      *this = *reinterpret_cast<NPCapability*>(bytes);
    }
  }

  int64_t pid() const { return pid_; }
  void set_pid(int64_t pid) { pid_ = pid; }

  NPObject* object() const {
    // TODO(sehr,ilewis): when assert_cast is ready on NaCl, use it here.
    return reinterpret_cast<NPObject*>(static_cast<uintptr_t>(object_));
  }
  void set_object(NPObject* object) {
    object_ = reinterpret_cast<uint64_t>(object);
  }

  // Copies the specified capability value to this capability.
  NPCapability& CopyFrom(const NPCapability& capability) {
    pid_ = capability.pid_;
    object_ = capability.object_;
    return *this;
  }

  char* char_addr() { return reinterpret_cast<char*>(this); }
  nacl_abi_size_t size() { return static_cast<nacl_abi_size_t>(sizeof(*this)); }

 private:
  uint64_t object_;  // The pointer to the object in the owner process.
  int64_t pid_;     // The process ID that has the object.
};

// Less (<) is required for the std::map template class.
inline bool operator<(const NPCapability& c1, const NPCapability& c2) {
  if (c1.pid() == c2.pid())
    return (c1.object() < c2.object());
  return (c2.pid() < c1.pid());
}

inline bool operator==(const NPCapability& c1, const NPCapability& c2) {
  return (c1.pid() == c2.pid() && c1.object() == c2.object());
}

}  // namespace nacl

#endif  // NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPCAPABILITY_H_
