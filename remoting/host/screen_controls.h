// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SCREEN_CONTROLS_H_
#define REMOTING_HOST_SCREEN_CONTROLS_H_

#include "base/basictypes.h"

namespace remoting {

class ScreenResolution;

// Used to change the screen resolution (both dimensions and DPI).
class ScreenControls {
 public:
  virtual ~ScreenControls() {}

  // Attempts to set new screen resolution in the session.
  virtual void SetScreenResolution(const ScreenResolution& resolution) = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_SCREEN_CONTROLS_H_
