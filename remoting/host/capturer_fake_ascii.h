// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CAPTURER_FAKE_ASCII_H_
#define REMOTING_HOST_CAPTURER_FAKE_ASCII_H_

#include "base/scoped_ptr.h"
#include "remoting/host/capturer.h"

namespace remoting {

// A CapturerFakeAscii always outputs an image of 64x48 ASCII characters.
// This image is artificially generated for testing purpose.
//
// CapturerFakeAscii is doubled buffered as required by Capturer. See
// remoting/host/capturer.h.
class CapturerFakeAscii : public Capturer {
 public:
  explicit CapturerFakeAscii(MessageLoop* message_loop);
  virtual ~CapturerFakeAscii();

  virtual void ScreenConfigurationChanged();

 private:
  virtual void CalculateInvalidRects();
  virtual void CaptureRects(const InvalidRects& rects,
                            CaptureCompletedCallback* callback);

  // Generates an image in the front buffer.
  void GenerateImage();

  // We have two buffers for the screen images as required by Capturer.
  scoped_array<uint8> buffers_[kNumBuffers];

  DISALLOW_COPY_AND_ASSIGN(CapturerFakeAscii);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CAPTURER_FAKE_ASCII_H_
