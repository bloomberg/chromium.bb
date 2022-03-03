// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PROFILING_UTILS_H_
#define CONTENT_PUBLIC_BROWSER_PROFILING_UTILS_H_

#include "content/common/content_export.h"

namespace content {

// Ask all the child processes to dump their profiling data to disk and block
// until this is done.
CONTENT_EXPORT void WaitForAllChildrenToDumpProfilingData();

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PROFILING_UTILS_H_
