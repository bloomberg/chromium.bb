// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_NEARBY_LIBRARY_ATOMIC_REFERENCE_H_
#define CHROMEOS_COMPONENTS_NEARBY_LIBRARY_ATOMIC_REFERENCE_H_

namespace location {
namespace nearby {

// An object reference that may be updated atomically.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/atomic/AtomicReference.html
template <typename T>
class AtomicReference {
 public:
  virtual ~AtomicReference() {}

  virtual T get() = 0;
  virtual void set(T value) = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // CHROMEOS_COMPONENTS_NEARBY_LIBRARY_ATOMIC_REFERENCE_H__
