// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/breakpad.h"

namespace remoting {

void InitializeCrashReporting() {
  // Crash dump collection is not implemented on Linux yet.
  // http://crbug.com/130678.
}

}  // namespace remoting
