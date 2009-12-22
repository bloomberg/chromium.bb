// Copyright (c) 2008 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NaCl-NPAPI Interface

#ifndef NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPCAPABILITY_H_
#define NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPCAPABILITY_H_

#include "native_client/src/shared/npruntime/nacl_npapi.h"

namespace nacl {

struct NPCapability {
  // If an NPCapability represents a NaCl resource handle rather than an
  // NPObject, the pid field is set to kHandle.
  static const int kHandle = -1;

  int pid;           // The process ID that has the object.
  NPObject* object;  // The pointer to the object in the owner process.

  // Copies the specified capability value to this capability.
  NPCapability& CopyFrom(const NPCapability& capability) {
    pid = capability.pid;
    object = capability.object;
    return *this;
  }
};

// Less (<) is required for the std::map template class.
inline bool operator<(const NPCapability& c1, const NPCapability& c2) {
  if (c1.pid < c2.pid) {
    return true;
  }
  if (c2.pid < c1.pid) {
    return false;
  }
  if (c1.object < c2.object) {
    return true;
  }
  return false;
}

inline bool operator==(const NPCapability& c1, const NPCapability& c2) {
  return (c1.pid == c2.pid && c1.object == c2.object) ? true : false;
}

}  // namespace nacl

#endif  // NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPCAPABILITY_H_
