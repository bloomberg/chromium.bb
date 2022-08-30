// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_IME_IME_SANDBOX_HOOK_H_
#define ASH_SERVICES_IME_IME_SANDBOX_HOOK_H_

#include "sandbox/policy/linux/sandbox_linux.h"

namespace ash {
namespace ime {

bool ImePreSandboxHook(sandbox::policy::SandboxLinux::Options options);

}  // namespace ime
}  // namespace ash

#endif  // ASH_SERVICES_IME_IME_SANDBOX_HOOK_H_
