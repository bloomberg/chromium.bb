// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CAPTURER_FAKE_H_
#define REMOTING_HOST_CAPTURER_FAKE_H_

#include "base/scoped_ptr.h"
#include "remoting/host/capturer.h"

namespace remoting {

// A CapturerFake generates artificial image for testing purpose.
//
// CapturerFake is doubled buffered as required by Capturer. See
// remoting/host/capturer.h.
class CapturerFake : public Capturer {
 public:
  explicit CapturerFake(MessageLoop* message_loop);
  virtual ~CapturerFake();

  virtual void ScreenConfigurationChanged();

 private:
  virtual void CalculateInvalidRects();
  virtual void CaptureRects(const InvalidRects& rects,
                            CaptureCompletedCallback* callback);

  // Generates an image in the front buffer.
  void GenerateImage();

  gfx::Size size_;
  int bytes_per_row_;
  int box_pos_x_;
  int box_pos_y_;
  int box_speed_x_;
  int box_speed_y_;

  // We have two buffers for the screen images as required by Capturer.
  scoped_array<uint8> buffers_[kNumBuffers];

  DISALLOW_COPY_AND_ASSIGN(CapturerFake);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CAPTURER_FAKE_H_
