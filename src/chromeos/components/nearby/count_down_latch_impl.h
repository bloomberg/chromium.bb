// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_NEARBY_COUNT_DOWN_LATCH_IMPL_H_
#define CHROMEOS_COMPONENTS_NEARBY_COUNT_DOWN_LATCH_IMPL_H_

#include "base/atomic_ref_count.h"
#include "base/macros.h"
#include "base/synchronization/waitable_event.h"
#include "chromeos/components/nearby/library/count_down_latch.h"
#include "chromeos/components/nearby/library/exception.h"

namespace chromeos {

namespace nearby {

// Concrete location::nearby::CountDownLatch implementation.
class CountDownLatchImpl : public location::nearby::CountDownLatch {
 public:
  explicit CountDownLatchImpl(int32_t count);
  ~CountDownLatchImpl() override;

 private:
  // location::nearby::CountDownLatch:
  location::nearby::Exception::Value await() override;
  location::nearby::ExceptionOr<bool> await(int32_t timeout_millis) override;
  void countDown() override;

  base::AtomicRefCount count_;
  base::WaitableEvent count_waitable_event_;

  DISALLOW_COPY_AND_ASSIGN(CountDownLatchImpl);
};

}  // namespace nearby

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_NEARBY_COUNT_DOWN_LATCH_IMPL_H_
