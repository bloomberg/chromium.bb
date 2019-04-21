// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/worker_thread_util.h"

#include "content/public/renderer/worker_thread.h"
#include "extensions/common/constants.h"

namespace extensions {
namespace worker_thread_util {

bool IsWorkerThread() {
  return content::WorkerThread::GetCurrentId() != kMainThreadId;
}

}  // namespace worker_thread_util
}  // namespace extensions
