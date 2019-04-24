// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/threading.h"

#include "build/build_config.h"

namespace WTF {

base::PlatformThreadId CurrentThread() {
  thread_local base::PlatformThreadId g_id = base::PlatformThread::CurrentId();
  return g_id;
}

// For debugging only -- whether a non-main thread has been created.
// No synchronization is required, since this is called before any such thread
// exists.

#if DCHECK_IS_ON()
static bool g_thread_created = false;

bool IsBeforeThreadCreated() {
  return !g_thread_created;
}

void WillCreateThread() {
  g_thread_created = true;
}
#endif

}  // namespace WTF
