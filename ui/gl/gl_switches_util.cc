// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_switches_util.h"

#include "base/command_line.h"
#include "base/macros.h"
#include "ui/gl/gl_switches.h"

namespace gl {

bool IsPresentationCallbackEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnablePresentationCallback);
}

}  // namespace gl
