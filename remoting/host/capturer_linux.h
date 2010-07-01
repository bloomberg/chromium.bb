// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CAPTURER_LINUX_H_
#define REMOTING_HOST_CAPTURER_LINUX_H_

#include "remoting/host/capturer.h"

namespace remoting {

// A class to perform capturing for Linux.
class CapturerLinux : public Capturer {
 public:
  CapturerLinux();
  virtual ~CapturerLinux();

  virtual void CaptureRects(const RectVector& rects,
                            CaptureCompletedCallback* callback);

  virtual void ScreenConfigurationChanged();

 private:

  DISALLOW_COPY_AND_ASSIGN(CapturerLinux);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CAPTURER_LINUX_H_
