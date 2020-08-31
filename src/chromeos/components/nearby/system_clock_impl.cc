// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/nearby/system_clock_impl.h"

#include "base/system/sys_info.h"

namespace chromeos {

namespace nearby {

SystemClockImpl::SystemClockImpl() = default;

SystemClockImpl::~SystemClockImpl() = default;

int64_t SystemClockImpl::elapsedRealtime() {
  // TODO(kyleqian): The POSIX implementation of base::SysInfo::Uptime()
  // currently does not include time spent suspended. See
  // https://crbug.com/166153.
  return base::SysInfo::Uptime().InMilliseconds();
}

}  // namespace nearby

}  // namespace chromeos
