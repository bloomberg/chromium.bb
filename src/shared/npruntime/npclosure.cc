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

class NPClosureTable::NPClosure {
 public:
  NPClosure(FunctionPointer func, void* user_data) :
    func_(func), user_data_(user_data) {
  }

  FunctionPointer func() const { return func_; }
  void* user_data() const { return user_data_; }

 private:
  FunctionPointer func_;
  void* user_data_;
};

bool NPClosureTable::Add(FunctionPointer func,
                         void* user_data,
                         uint32_t* id) {
  bool retval = false;

  // Create the closure.
  NPClosure* closure = new(std::nothrow) NPClosure(func, user_data);
  if (NULL == closure) {
    // Not enough memory to create the closure.
    return false;
  }
  // Ensure accesses to the map are mutually exclusive.
  pthread_mutex_lock(&mu_);
  // Add the closure to the pending map.  After re-use begins to happen, we
  // need to make sure that we're not reusing a live slot.
  const uint32_t kMaxTries = 16;
  uint32_t tries;
  for (tries = 0; tries < kMaxTries; ++tries, ++next_id_) {
    ClosureMap::iterator i = closures_.find(next_id_);
    if (closures_.end() == i) {
      break;
    }
  }
  if (kMaxTries <= tries) {
    // Too many retries in the table.
    delete closure;
    goto done;
  }
  closures_[next_id_] = closure;
  *id = next_id_;
  retval = true;
 done:
  pthread_mutex_unlock(&mu_);
  return retval;
}

bool NPClosureTable::Remove(uint32_t key,
                            FunctionPointer* func,
                            void** user_data) {
  NPClosure* closure = NULL;
  bool retval = false;

  // Ensure accesses to the map are mutually exclusive.
  pthread_mutex_lock(&mu_);
  // Look up the closure for the given key.
  *user_data = NULL;
  ClosureMap::iterator i = closures_.find(key);
  if (closures_.end() == i) {
    // No corresponding closure existed.
    goto done;
  }
  closure = i->second;
  *func = closure->func();
  *user_data = closure->user_data();
  closures_.erase(i);
  retval = true;
 done:
  pthread_mutex_unlock(&mu_);
  delete closure;
  return retval;
}

}  // namespace nacl
