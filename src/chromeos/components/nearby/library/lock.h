// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_NEARBY_LIBRARY_LOCK_H_
#define CHROMEOS_COMPONENTS_NEARBY_LIBRARY_LOCK_H_

namespace location {
namespace nearby {

// A lock is a tool for controlling access to a shared resource by multiple
// threads.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/locks/Lock.html
class Lock {
 public:
  virtual ~Lock() {}

  virtual void lock() = 0;
  virtual void unlock() = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // CHROMEOS_COMPONENTS_NEARBY_LIBRARY_LOCK_H_
