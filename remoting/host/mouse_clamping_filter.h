// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_MOUSE_CLAMPING_FILTER_H_
#define REMOTING_HOST_MOUSE_CLAMPING_FILTER_H_

#include "base/compiler_specific.h"
#include "remoting/protocol/mouse_input_filter.h"

namespace remoting {

class VideoFrameCapturer;

// Filtering InputStub implementation which clamps mouse each mouse event to
// the current dimensions of a VideoFrameCapturer instance before passing
// them on to the target |input_stub|.
class MouseClampingFilter : public protocol::MouseInputFilter {
 public:
  MouseClampingFilter(VideoFrameCapturer* capturer,
                      protocol::InputStub* input_stub);
  virtual ~MouseClampingFilter();

  // InputStub overrides.
  virtual void InjectMouseEvent(const protocol::MouseEvent& event) OVERRIDE;

 private:
  VideoFrameCapturer* capturer_;

  DISALLOW_COPY_AND_ASSIGN(MouseClampingFilter);
};

}  // namespace remoting

#endif  // REMOTING_HOST_MOUSE_CLAMPING_FILTER_H_
