// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_NETWORK_SANDBOX_HOOK_LINUX_H_
#define SERVICES_NETWORK_NETWORK_SANDBOX_HOOK_LINUX_H_

#include "services/service_manager/sandbox/linux/sandbox_linux.h"

namespace network {

bool NetworkPreSandboxHook(service_manager::SandboxLinux::Options options);

}  // namespace network

#endif  // SERVICES_NETWORK_NETWORK_SANDBOX_HOOK_LINUX_H_
