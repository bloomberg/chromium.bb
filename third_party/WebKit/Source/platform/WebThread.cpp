// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "public/platform/WebThread.h"

#include "wtf/Assertions.h"

#if OS(WIN)
#include <windows.h>
#elif OS(POSIX)
#include <unistd.h>
#endif

namespace {
#if OS(WIN)
COMPILE_ASSERT(sizeof(blink::PlatformThreadId) >= sizeof(DWORD), Size_of_platform_thread_id_is_too_small);
#elif OS(POSIX)
COMPILE_ASSERT(sizeof(blink::PlatformThreadId) >= sizeof(pid_t), Size_of_platform_thread_id_is_too_small);
#else
#error Unexpected platform
#endif
}
