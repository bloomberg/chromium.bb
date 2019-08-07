// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_NEARBY_SETTABLE_FUTURE_IMPL_H_
#define CHROMEOS_COMPONENTS_NEARBY_SETTABLE_FUTURE_IMPL_H_

#include "base/macros.h"
#include "base/optional.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "chromeos/components/nearby/library/exception.h"
#include "chromeos/components/nearby/library/settable_future.h"

namespace chromeos {

namespace nearby {

// Concrete location::nearby::SettableFuture implementation.
template <typename T>
class SettableFutureImpl : public location::nearby::SettableFuture<T> {
 public:
  SettableFutureImpl() = default;
  ~SettableFutureImpl() override = default;

  // This method will unblock any outstanding calls to get(), causing them to
  // return |value|. Any future calls to get() will also return |value|. This
  // operation can only successfully run one time, and subsequent calls will
  // return false and will not update the ExceptionOr returned to get()ers.
  bool SetExceptionOr(const location::nearby::ExceptionOr<T>& value) {
    base::AutoLock al(set_value_lock_);
    if (!value_has_been_set_.IsSignaled()) {
      value_ = value;
      value_has_been_set_.Signal();
      return true;
    }

    return false;
  }

 private:
  friend class SettableFutureImplTest;

  // location::nearby::Future:
  location::nearby::ExceptionOr<T> get() override {
    if (!value_has_been_set_.IsSignaled())
      value_has_been_set_.Wait();

    return *value_;
  }

  // location::nearby::SettableFuture:
  bool set(T value) override {
    return SetExceptionOr(location::nearby::ExceptionOr<T>(value));
  }

  base::Optional<location::nearby::ExceptionOr<T>> value_;

  // This Lock is used to make sure that within set(), setting |value_| and
  // signaling |value_has_been_set_| happens atomically, such that subsequent
  // calls to set() will be guaranteed to neither set |value_| nor Signal().
  base::Lock set_value_lock_;
  base::WaitableEvent value_has_been_set_;

  DISALLOW_COPY_AND_ASSIGN(SettableFutureImpl);
};

}  // namespace nearby

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_NEARBY_SETTABLE_FUTURE_IMPL_H_
