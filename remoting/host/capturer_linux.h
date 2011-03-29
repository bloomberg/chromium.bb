// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CAPTURER_LINUX_H_
#define REMOTING_HOST_CAPTURER_LINUX_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/host/capturer.h"

namespace remoting {

class CapturerLinuxPimpl;

// A class to perform capturing for Linux.
class CapturerLinux : public Capturer {
 public:
  CapturerLinux();
  virtual ~CapturerLinux();

  // Capturer interface.
  virtual void ScreenConfigurationChanged();
  virtual media::VideoFrame::Format pixel_format() const;
  virtual void ClearInvalidRects();
  virtual void InvalidateRects(const InvalidRects& inval_rects);
  virtual void InvalidateScreen(const gfx::Size& size);
  virtual void InvalidateFullScreen();
  virtual void CaptureInvalidRects(CaptureCompletedCallback* callback);
  virtual const gfx::Size& size_most_recent() const;

 private:
  friend class CapturerLinuxPimpl;

  scoped_ptr<CapturerLinuxPimpl> pimpl_;
  DISALLOW_COPY_AND_ASSIGN(CapturerLinux);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CAPTURER_LINUX_H_
