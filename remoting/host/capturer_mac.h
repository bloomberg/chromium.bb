// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CAPTURER_MAC_H_
#define REMOTING_HOST_CAPTURER_MAC_H_

#include "remoting/host/capturer.h"

namespace remoting {

// A class to perform capturing for mac.
class CapturerMac : public Capturer {
 public:
  CapturerMac();
  virtual ~CapturerMac();

  virtual void CaptureFullScreen(Task* done_task);
  virtual void CaptureDirtyRects(Task* done_task);
  virtual void CaptureRect(const gfx::Rect& rect, Task* done_task);
  virtual void GetData(const uint8* planes[]) const;
  virtual void GetDataStride(int strides[]) const;

 private:
  // Generates an image in the current buffer.
  void CaptureImage();

  DISALLOW_COPY_AND_ASSIGN(CapturerMac);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CAPTURER_MAC_H_
