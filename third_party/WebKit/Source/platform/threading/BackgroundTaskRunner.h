// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BackgroundTaskRunner_h
#define BackgroundTaskRunner_h

#include "base/location.h"
#include "platform/PlatformExport.h"
#include "platform/wtf/Functional.h"

namespace blink {

namespace BackgroundTaskRunner {

PLATFORM_EXPORT void PostOnBackgroundThread(const base::Location&,
                                            CrossThreadClosure);

}  // BackgroundTaskRunner

}  // namespace blink

#endif
