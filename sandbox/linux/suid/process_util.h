// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The following is duplicated from base/process_utils.h.
// We shouldn't link against C++ code in a setuid binary.

#ifndef SANDBOX_LINUX_SUID_PROCESS_UTIL_H_
#define SANDBOX_LINUX_SUID_PROCESS_UTIL_H_

#include <stdbool.h>
#include <sys/types.h>

#include "base/base_export.h"

static const char kAdjustOOMScoreSwitch[] = "--adjust-oom-score";

// This adjusts /proc/process/oom_adj so the Linux OOM killer will prefer
// certain process types over others. The range for the adjustment is
// [-17,15], with [0,15] being user accessible.
BASE_EXPORT bool AdjustOOMScore(pid_t process, int score);

#endif  // SANDBOX_LINUX_SUID_PROCESS_UTIL_H_
