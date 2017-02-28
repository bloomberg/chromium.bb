// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UseCounterCallback_h
#define UseCounterCallback_h

#include "core/CoreExport.h"
#include "v8/include/v8.h"

namespace blink {

// Callback that is used to count the number of times a V8 feature is used.
CORE_EXPORT void useCounterCallback(v8::Isolate*,
                                    v8::Isolate::UseCounterFeature);

}  // namespace blink

#endif  // UseCounterCallback_h
