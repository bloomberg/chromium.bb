// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "trace_wrapper.h"

namespace blink {

void B::Trace(Visitor* visitor) {
  static_cast<C*>(this)->TraceAfterDispatch(visitor);
}

}  // namespace blink
