// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/mouse_input_filter.h"

#include <algorithm>

#include "base/logging.h"
#include "base/numerics/ranges.h"
#include "remoting/proto/event.pb.h"

namespace remoting {
namespace protocol {

MouseInputFilter::MouseInputFilter() = default;

MouseInputFilter::MouseInputFilter(InputStub* input_stub)
    : InputFilter(input_stub) {
}

MouseInputFilter::~MouseInputFilter() = default;

void MouseInputFilter::InjectMouseEvent(const MouseEvent& event) {
  if (input_size_.IsEmpty() || output_size_.IsEmpty())
    return;

  // We scale based on the maximum input & output coordinates, rather than the
  // input and output sizes, so that it's possible to reach the edge of the
  // output when up-scaling.  We also take care to round up or down correctly,
  // which is important when down-scaling.
  // After scaling, we offset by the output rect origin. This is normally (0,0),
  // but will be non-zero when we are showing a single display.
  MouseEvent out_event(event);
  if (out_event.has_x()) {
    int x = out_event.x() * output_size_.WidthAsPixels();
    x = (x + input_size_.WidthAsDips() / 2) / input_size_.WidthAsDips();
    out_event.set_x(output_offset_.x() +
                    base::ClampToRange(x, 0, output_size_.WidthAsPixels()));
  }
  if (out_event.has_y()) {
    int y = out_event.y() * output_size_.HeightAsPixels();
    y = (y + input_size_.HeightAsDips() / 2) / input_size_.HeightAsDips();
    out_event.set_y(output_offset_.y() +
                    base::ClampToRange(y, 0, output_size_.HeightAsPixels()));
  }

  InputFilter::InjectMouseEvent(out_event);
}

void MouseInputFilter::set_input_size(const DisplaySize& size) {
  input_size_ = DisplaySize(size.WidthAsDips() - 1, size.HeightAsDips() - 1,
                            size.GetDpi());
  LOG(INFO) << "Setting MouseInputFilter input_size to (dips) " << input_size_;
}

void MouseInputFilter::set_output_size(const DisplaySize& size) {
  output_size_ = DisplaySize::FromPixels(
      size.WidthAsPixels() - 1, size.HeightAsPixels() - 1, size.GetDpi());
  LOG(INFO) << "Setting MouseInputFilter output_size to (px) "
            << output_size_.WidthAsPixels() << "x"
            << output_size_.HeightAsPixels();
}

void MouseInputFilter::set_output_offset(const webrtc::DesktopVector& v) {
  output_offset_ = webrtc::DesktopVector(v.x(), v.y());
  LOG(INFO) << "Setting MouseInputFilter output_offset to "
            << output_offset_.x() << "," << output_offset_.y();
}

}  // namespace protocol
}  // namespace remoting
