// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BlinkInitializer_h
#define BlinkInitializer_h

#include "controller/ControllerExport.h"

namespace blink {

class Platform;

// The embedder must call this function before using blink.
CONTROLLER_EXPORT void InitializeBlink(Platform*);

}  // namespace blink

#endif  // BlinkInitializer_h
