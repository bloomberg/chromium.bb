// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SUID_SANDBOX_H_
#define SANDBOX_LINUX_SUID_SANDBOX_H_
#pragma once

#if defined(__cplusplus)
namespace sandbox {
#endif

// These are command line switches that may be used by other programs
// (e.g. Chrome) to construct a command line for the sandbox.
static const char kAdjustOOMScoreSwitch[] = "--adjust-oom-score";
#if defined(OS_CHROMEOS)
static const char kAdjustLowMemMarginSwitch[] = "--adjust-low-mem";
#endif

#if defined(__cplusplus)
}  // namespace sandbox
#endif

#endif  // SANDBOX_LINUX_SUID_SANDBOX_H_
