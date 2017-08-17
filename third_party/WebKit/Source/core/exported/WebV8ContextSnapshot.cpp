// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/web/WebV8ContextSnapshot.h"

#include "bindings/core/v8/V8ContextSnapshot.h"
#include "v8/include/v8.h"

namespace blink {

v8::StartupData WebV8ContextSnapshot::TakeSnapshot() {
  return V8ContextSnapshot::TakeSnapshot();
}

}  // namespace blink
