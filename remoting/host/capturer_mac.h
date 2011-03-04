// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CAPTURER_MAC_H_
#define REMOTING_HOST_CAPTURER_MAC_H_

#include "remoting/host/capturer.h"
#include <ApplicationServices/ApplicationServices.h>
#include <OpenGL/OpenGL.h>
#include "base/scoped_ptr.h"

namespace remoting {

// A class to perform capturing for mac.
class CapturerMac : public Capturer {
 public:
  explicit CapturerMac(MessageLoop* message_loop);
  virtual ~CapturerMac();

  virtual void ScreenConfigurationChanged();

 private:
  virtual void CalculateInvalidRects();
  virtual void CaptureRects(const InvalidRects& rects,
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
  scoped_array<uint8> buffers_[kNumBuffers];
  scoped_array<uint8> flip_buffer_;

  // Screen size.
  int width_;
  int height_;

  int bytes_per_row_;

  DISALLOW_COPY_AND_ASSIGN(CapturerMac);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CAPTURER_MAC_H_
