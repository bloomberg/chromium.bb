// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebV8ContextSnapshot_h
#define WebV8ContextSnapshot_h

#include "public/platform/WebCommon.h"
#include "v8/include/v8.h"

namespace blink {

// WebV8ContextSnapshot is an API to take a snapshot of V8 context.
// This API should be used only by tools/v8_context_snapshot, which runs during
// Chromium's build step.
class BLINK_EXPORT WebV8ContextSnapshot {
 public:
  static v8::StartupData TakeSnapshot();
};

}  // namespace blink

#endif  // WebV8ContextSnapshot_h
