// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/breakpad.h"

namespace remoting {

void InitializeCrashReporting() {
  // Do nothing because crash dump reporting on Mac is initialized from
  // awakeFromNib method.
}

}  // namespace remoting
