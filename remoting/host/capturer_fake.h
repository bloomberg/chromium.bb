// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CAPTURER_FAKE_H_
#define REMOTING_HOST_CAPTURER_FAKE_H_

#include "base/memory/scoped_ptr.h"
#include "remoting/host/capturer.h"
#include "remoting/host/capturer_helper.h"

namespace remoting {

// A CapturerFake generates artificial image for testing purpose.
//
// CapturerFake is double-buffered as required by Capturer. See
// remoting/host/capturer.h.
class CapturerFake : public Capturer {
 public:
  CapturerFake();
  virtual ~CapturerFake();

  // Capturer interface.
  virtual void ScreenConfigurationChanged() OVERRIDE;
  virtual media::VideoFrame::Format pixel_format() const OVERRIDE;
  virtual void ClearInvalidRegion() OVERRIDE;
  virtual void InvalidateRegion(const SkRegion& invalid_region) OVERRIDE;
  virtual void InvalidateScreen(const gfx::Size& size) OVERRIDE;
  virtual void InvalidateFullScreen() OVERRIDE;
  virtual void CaptureInvalidRegion(CaptureCompletedCallback* callback)
      OVERRIDE;
  virtual const gfx::Size& size_most_recent() const OVERRIDE;

 private:
  // Generates an image in the front buffer.
  void GenerateImage();

  gfx::Size size_;
  int bytes_per_row_;
  int box_pos_x_;
  int box_pos_y_;
  int box_speed_x_;
  int box_speed_y_;

  CapturerHelper helper;

  // We have two buffers for the screen images as required by Capturer.
  static const int kNumBuffers = 2;
  scoped_array<uint8> buffers_[kNumBuffers];

  // The current buffer with valid data for reading.
  int current_buffer_;

  // Format of pixels returned in buffer.
  media::VideoFrame::Format pixel_format_;

  DISALLOW_COPY_AND_ASSIGN(CapturerFake);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CAPTURER_FAKE_H_
