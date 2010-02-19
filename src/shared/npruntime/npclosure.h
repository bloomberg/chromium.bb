// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NaCl-NPAPI Interface

#ifndef NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPCLOSURE_H_
#define NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPCLOSURE_H_

#ifndef __native_client__
#error "This file is only for inclusion in Native Client modules"
#endif  // __native_client__

#include <pthread.h>
#include <map>

namespace nacl {

// A class maintaining a synchronized table of closures.
class NPClosureTable {
 public:
  // A typedef used for function pointers to be used in closures.
  typedef void (*FunctionPointer)(void* user_data);

  NPClosureTable() {
    pthread_mutex_init(&mu_, NULL);
    next_id_ = 0;
  }

  ~NPClosureTable() {
    pthread_mutex_destroy(&mu_);
  }

  // Add a closure consisting of func and user_data to the table.  If
  // successful, it returns true, and id can be used to remove the closure
  // from the table.
  bool Add(FunctionPointer func, void* user_data, uint32_t* id);

  // Remove the closure for id from the table.  If successful, it returns
  // true and sets func and user_data.
  bool Remove(uint32_t id, FunctionPointer* func, void** user_data);

 private:
  // The class used to represent a closure.
  class NPClosure;

  // The type of the mapping used to keep track of the live closures.
  typedef std::map<uint32_t, NPClosure*> ClosureMap;

  // The map of pending closures.
  ClosureMap closures_;
  // The identifier of the next closure to be created.
  uint32_t next_id_;
  // The lock that guards insertion/deletion in the table of live closures.
  pthread_mutex_t mu_;
};

}  // namespace nacl

#endif  // NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPCLOSURE_H_
