// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// See port_example.h for documentation for the following types/functions.

#ifndef STORAGE_LEVELDB_PORT_PORT_CHROMIUM_H_
#define STORAGE_LEVELDB_PORT_PORT_CHROMIUM_H_

#include <cstddef>
#include <cstdint>
#include <string>

#include "base/check.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"

namespace leveldb {
namespace port {

class LOCKABLE Mutex {
 public:
  Mutex() = default;
  ~Mutex() = default;

  Mutex(const Mutex&) = delete;
  Mutex& operator=(const Mutex&) = delete;

  void Lock() EXCLUSIVE_LOCK_FUNCTION() { lock_.Acquire(); }
  void Unlock() UNLOCK_FUNCTION() { lock_.Release(); }
  void AssertHeld() ASSERT_EXCLUSIVE_LOCK() { lock_.AssertAcquired(); }

 private:
  friend class CondVar;
  base::Lock lock_;
};

// Thinly wraps std::condition_variable.
class CondVar {
 public:
  explicit CondVar(Mutex* mu) : cv_(&mu->lock_) { DCHECK(mu); }
  ~CondVar() = default;

  CondVar(const CondVar&) = delete;
  CondVar& operator=(const CondVar&) = delete;

  void Wait() { cv_.Wait(); }
  void Signal() { cv_.Signal(); }
  void SignalAll() { cv_.Broadcast(); }

 private:
  base::ConditionVariable cv_;
};

bool Snappy_Compress(const char* input, size_t input_length,
                     std::string* output);
bool Snappy_GetUncompressedLength(const char* input, size_t length,
                                  size_t* result);
bool Snappy_Uncompress(const char* input_data, size_t input_length,
                       char* output);

inline bool GetHeapProfile(void (*func)(void*, const char*, int), void* arg) {
  return false;
}

uint32_t AcceleratedCRC32C(uint32_t crc, const char* buf, size_t size);

}  // namespace port
}  // namespace leveldb

#endif  // STORAGE_LEVELDB_PORT_PORT_CHROMIUM_H_
