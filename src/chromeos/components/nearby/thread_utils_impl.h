// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_NEARBY_THREAD_UTILS_IMPL_H_
#define CHROMEOS_COMPONENTS_NEARBY_THREAD_UTILS_IMPL_H_

#include "base/macros.h"
#include "chromeos/components/nearby/library/exception.h"
#include "chromeos/components/nearby/library/thread_utils.h"

namespace chromeos {

namespace nearby {

// Concrete location::nearby::ThreadUtils implementation.
class ThreadUtilsImpl : public location::nearby::ThreadUtils {
 public:
  ThreadUtilsImpl();
  ~ThreadUtilsImpl() override;

 private:
  // location::nearby::ThreadUtils:
  location::nearby::Exception::Value sleep(int64_t millis) override;

  DISALLOW_COPY_AND_ASSIGN(ThreadUtilsImpl);
};

}  // namespace nearby

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_NEARBY_THREAD_UTILS_IMPL_H_
