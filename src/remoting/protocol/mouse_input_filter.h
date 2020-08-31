// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_MOUSE_INPUT_FILTER_H_
#define REMOTING_PROTOCOL_MOUSE_INPUT_FILTER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "remoting/protocol/display_size.h"
#include "remoting/protocol/input_filter.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {
namespace protocol {

// Filtering InputStub implementation which scales mouse events based on the
// supplied input and output dimensions, and clamps their coordinates to the
// output dimensions, before passing events on to |input_stub|.
class MouseInputFilter : public InputFilter {
 public:
  MouseInputFilter();
  explicit MouseInputFilter(InputStub* input_stub);
  ~MouseInputFilter() override;

  // Specify the input dimensions (DIPs or pixels) for mouse events.
  // Deoending on the protocol, the input size can be in either pixels (for
  // ICE protocol) or DIPs (for webrtc - except for Mac, which is pixels).
  void set_input_size(const int32_t x, const int32_t y);

  // Specify the output dimensions (DIPs or pixels, depending on platform).
  // The output size is the scale that we should apply to the incoming mouse
  // events before injecting them using the platform native APIs.
  void set_output_size(const int32_t x, const int32_t y);

  // Offset to display origin, in DIPs or pixels depending on the native
  // platform. This will typically be (0,0), but may take on other values
  // when there are multiple displays.
  void set_output_offset(const webrtc::DesktopVector& v);

  // InputStub overrides.
  void InjectMouseEvent(const protocol::MouseEvent& event) override;

 private:
  int32_t x_input_, y_input_;
  int32_t x_output_, y_output_;

  webrtc::DesktopVector output_offset_;

  DISALLOW_COPY_AND_ASSIGN(MouseInputFilter);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_MOUSE_INPUT_FILTER_H_
