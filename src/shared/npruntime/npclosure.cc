// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NaCl-NPAPI Interface

#include <inttypes.h>
#include <stdint.h>
#include <nacl/nacl_inttypes.h>
#include <new>
#include "native_client/src/shared/npruntime/npclosure.h"

namespace nacl {

uint32_t NPClosureTable::Add(NPClosure* closure) {
  uint32_t id = kInvalidId;

  // Ensure accesses to the map are mutually exclusive.
  pthread_mutex_lock(&mu_);
  // Add the closure to the pending map.  After re-use begins to happen, we
  // need to make sure that we're not reusing a live slot.
  const uint32_t kMaxTries = 16;
  uint32_t tries;
  for (tries = 0; tries < kMaxTries; ++tries, ++next_id_) {
    if (kInvalidId == next_id_) {
      // We reserve kInvalidId for error returns.
      // Check for when next_id_ == kInvalidId and skip to the next.
      continue;
    }
    ClosureMap::iterator i = closures_.find(next_id_);
    if (closures_.end() == i) {
      break;
    }
  }
  if (kMaxTries <= tries) {
    // Too many retries in the table.
    goto done;
  }
  closures_[next_id_] = closure;
  id = next_id_;
 done:
  pthread_mutex_unlock(&mu_);
  return id;
}

NPClosure* NPClosureTable::Remove(uint32_t id) {
  NPClosure* closure = NULL;

  // Ensure accesses to the map are mutually exclusive.
  pthread_mutex_lock(&mu_);
  // Look up the closure for the given id.
  ClosureMap::iterator iter = closures_.find(id);
  if (closures_.end() == iter) {
    // No corresponding closure existed.
    goto done;
  }
  closure = iter->second;
  // Remove closure from the map.
  closures_.erase(iter);
 done:
  pthread_mutex_unlock(&mu_);
  return closure;
}

}  // namespace nacl
