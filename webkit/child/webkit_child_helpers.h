// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_CHILD_WEBKIT_CHILD_HELPERS_H_
#define WEBKIT_CHILD_WEBKIT_CHILD_HELPERS_H_

#include "base/basictypes.h"
#include "webkit/child/webkit_child_export.h"

namespace webkit_glue {

// Returns an estimate of the memory usage of the renderer process. Different
// platforms implement this function differently, and count in different
// allocations. Results are not comparable across platforms. The estimate is
// computed inside the sandbox and thus its not always accurate.
WEBKIT_CHILD_EXPORT size_t MemoryUsageKB();

}  // webkit_glue

#endif  // WEBKIT_CHILD_WEBKIT_CHILD_HELPERS_H_
