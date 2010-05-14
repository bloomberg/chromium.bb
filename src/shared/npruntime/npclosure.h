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

// A class representing a closure.
class NPClosure {
 public:
  // A typedef used for function pointers to be used in closures.
  typedef void (*FunctionPointer)(void* user_data);

  NPClosure(FunctionPointer func, void* data) :
    func_(func), data_(data) {
  }
  virtual ~NPClosure() { }

  virtual bool Run() {
    (*func_)(data_);
    return true;
  }

 protected:
  FunctionPointer func_;
  void* data_;

 private:
  // TODO(sehr): we need a DISALLOW_COPY_AND_ASSIGN here.
  NPClosure(const NPClosure&);
  void operator=(const NPClosure&);
};

// A class maintaining a synchronized table of closures.
class NPClosureTable {
 public:
  static const uint32_t kInvalidId = static_cast<uint32_t>(-1);

  NPClosureTable() {
    pthread_mutex_init(&mu_, NULL);
    next_id_ = 0;
  }

  ~NPClosureTable() {
    pthread_mutex_destroy(&mu_);
  }

  // Add a closure to the table.  If successful, the table takes ownership
  // of the closure, and this call returns the index used to remove the
  // closure from the table.  On failure it returns kInvalidId.
  uint32_t Add(NPClosure* closure);

  // Remove the closure for id from the table.  If successful, it returns
  // a non-NULL closure pointer, and the client takes ownership of the closure.
  // Returns NULL on failure.
  NPClosure* Remove(uint32_t id);

 private:
  // TODO(sehr): we need a DISALLOW_COPY_AND_ASSIGN here.
  NPClosureTable(const NPClosureTable&);
  void operator=(const NPClosureTable&);
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
