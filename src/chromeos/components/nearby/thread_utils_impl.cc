// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/nearby/thread_utils_impl.h"

#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chromeos/components/nearby/library/exception.h"

namespace chromeos {

namespace nearby {

ThreadUtilsImpl::ThreadUtilsImpl() = default;

ThreadUtilsImpl::~ThreadUtilsImpl() = default;

location::nearby::Exception::Value ThreadUtilsImpl::sleep(int64_t millis) {
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(millis));
  return location::nearby::Exception::NONE;
}

}  // namespace nearby

}  // namespace chromeos
