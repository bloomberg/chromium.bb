// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SESSION_CONTROLLER_H_
#define REMOTING_HOST_SESSION_CONTROLLER_H_

#include "base/basictypes.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "third_party/skia/include/core/SkSize.h"

namespace remoting {

class ScreenResolution;

class SessionController {
 public:
  virtual ~SessionController() {}

  // Attempts to set new screen resolution in the session.
  virtual void SetScreenResolution(const ScreenResolution& resolution) = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_SESSION_CONTROLLER_H_
