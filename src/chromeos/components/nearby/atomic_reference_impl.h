// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_NEARBY_ATOMIC_REFERENCE_IMPL_H_
#define CHROMEOS_COMPONENTS_NEARBY_ATOMIC_REFERENCE_IMPL_H_

#include <utility>

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "chromeos/components/nearby/library/atomic_reference.h"

namespace chromeos {

namespace nearby {

// Concrete location::nearby::AtomicReference implementation.
template <typename T>
class AtomicReferenceImpl : public location::nearby::AtomicReference<T> {
 public:
  explicit AtomicReferenceImpl(T initial_value) : value_(initial_value) {}

  ~AtomicReferenceImpl() override = default;

 private:
  // location::nearby::AtomicReference:
  T get() override {
    base::AutoLock al(value_lock_);
    return value_;
  }

  // location::nearby::AtomicReference:
  void set(T value) override {
    base::AutoLock al(value_lock_);
    value_ = value;
  }

  T value_;
  base::Lock value_lock_;

  DISALLOW_COPY_AND_ASSIGN(AtomicReferenceImpl);
};

}  // namespace nearby

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_NEARBY_ATOMIC_REFERENCE_IMPL_H__
