// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CAPTURER_FAKE_H_
#define REMOTING_HOST_CAPTURER_FAKE_H_

#include "base/scoped_ptr.h"
#include "remoting/host/capturer.h"

namespace remoting {

// A CapturerFake always output an image of 640x480 in 24bit RGB. The image
// is artificially generated for testing purpose.
//
// CapturerFake is doubled buffered as required by Capturer. See
// remoting/host/capturer.h.
class CapturerFake : public Capturer {
 public:
  CapturerFake();
  virtual ~CapturerFake();

  virtual void CaptureFullScreen(Task* done_task);
  virtual void CaptureDirtyRects(Task* done_task);
  virtual void CaptureRect(const gfx::Rect& rect, Task* done_task);
  virtual void GetData(const uint8* planes[]) const;
  virtual void GetDataStride(int strides[]) const;

 private:
  // Generates an image in the front buffer.
  void GenerateImage();

  // The seed for generating the image.
  int seed_;

  // We have two buffers for the screen images as required by Capturer.
  scoped_array<uint8> buffers_[kNumBuffers];

  DISALLOW_COPY_AND_ASSIGN(CapturerFake);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CAPTURER_FAKE_H_
