// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_VIRTUAL_MACHINES_VIRTUAL_MACHINES_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_VIRTUAL_MACHINES_VIRTUAL_MACHINES_UTIL_H_

namespace virtual_machines {

// Whether running virtual machines on Chrome OS is allowed
// per enterprise policy.
bool AreVirtualMachinesAllowedByPolicy();

// Whether running virtual machines on Chrome OS is allowed per hard-coded
// release schedule.
bool AreVirtualMachinesAllowedByVersionAndChannel();

}  // namespace virtual_machines

#endif  // CHROME_BROWSER_CHROMEOS_VIRTUAL_MACHINES_VIRTUAL_MACHINES_UTIL_H_
