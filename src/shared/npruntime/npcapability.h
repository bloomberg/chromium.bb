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
