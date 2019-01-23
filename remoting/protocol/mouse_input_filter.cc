// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/mouse_input_filter.h"

#include <algorithm>

#include "base/logging.h"
#include "remoting/proto/event.pb.h"

namespace remoting {
namespace protocol {

MouseInputFilter::MouseInputFilter() = default;

MouseInputFilter::MouseInputFilter(InputStub* input_stub)
    : InputFilter(input_stub) {
}

MouseInputFilter::~MouseInputFilter() = default;

void MouseInputFilter::InjectMouseEvent(const MouseEvent& event) {
  if (input_rect_.is_empty() || output_rect_.is_empty())
    return;

  // We scale based on the maximum input & output coordinates, rather than the
  // input and output sizes, so that it's possible to reach the edge of the
  // output when up-scaling.  We also take care to round up or down correctly,
  // which is important when down-scaling.
  // After scaling, we offset by the output rect origin. This is normally (0,0),
  // but will be non-zero when we are showing a single display.
  MouseEvent out_event(event);
  if (out_event.has_x()) {
    int x = out_event.x() * output_rect_.width();
    x = (x + input_rect_.width() / 2) / input_rect_.width();
    out_event.set_x(output_rect_.left() +
                    std::max(0, std::min(output_rect_.width(), x)));
  }
  if (out_event.has_y()) {
    int y = out_event.y() * output_rect_.height();
    y = (y + input_rect_.height() / 2) / input_rect_.height();
    out_event.set_y(output_rect_.top() +
                    std::max(0, std::min(output_rect_.height(), y)));
  }

  InputFilter::InjectMouseEvent(out_event);
}

void MouseInputFilter::set_input_size(const webrtc::DesktopRect& rect) {
  input_rect_ = webrtc::DesktopRect::MakeXYWH(
      rect.left(), rect.top(), rect.width() - 1, rect.height() - 1);
}

void MouseInputFilter::set_output_size(const webrtc::DesktopRect& rect) {
  output_rect_ = webrtc::DesktopRect::MakeXYWH(
      rect.left(), rect.top(), rect.width() - 1, rect.height() - 1);
}

}  // namespace protocol
}  // namespace remoting
