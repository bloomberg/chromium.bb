// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CAPTURER_MAC_H_
#define REMOTING_HOST_CAPTURER_MAC_H_

#include "remoting/host/capturer.h"
#include "remoting/host/capturer_helper.h"
#include <ApplicationServices/ApplicationServices.h>
#include <OpenGL/OpenGL.h>
#include "base/memory/scoped_ptr.h"

namespace remoting {

// A class to perform capturing for mac.
class CapturerMac : public Capturer {
 public:
  CapturerMac();
  virtual ~CapturerMac();

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
  void CaptureRects(const InvalidRects& rects,
                    CaptureCompletedCallback* callback);

  void ScreenRefresh(CGRectCount count, const CGRect *rect_array);
  void ScreenUpdateMove(CGScreenUpdateMoveDelta delta,
                                size_t count,
                                const CGRect *rect_array);
  static void ScreenRefreshCallback(CGRectCount count,
                                    const CGRect *rect_array,
                                    void *user_parameter);
  static void ScreenUpdateMoveCallback(CGScreenUpdateMoveDelta delta,
                                       size_t count,
                                       const CGRect *rect_array,
                                       void *user_parameter);
  static void DisplaysReconfiguredCallback(CGDirectDisplayID display,
                                           CGDisplayChangeSummaryFlags flags,
                                           void *user_parameter);

  void ReleaseBuffers();
  CGLContextObj cgl_context_;
  static const int kNumBuffers = 2;
  scoped_array<uint8> buffers_[kNumBuffers];
  scoped_array<uint8> flip_buffer_;

  // A thread-safe list of invalid rectangles, and the size of the most
  // recently captured screen.
  CapturerHelper helper_;

  // Screen size.
  int width_;
  int height_;

  int bytes_per_row_;

  // The current buffer with valid data for reading.
  int current_buffer_;

  // Format of pixels returned in buffer.
  media::VideoFrame::Format pixel_format_;

  DISALLOW_COPY_AND_ASSIGN(CapturerMac);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CAPTURER_MAC_H_
