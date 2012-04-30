// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_MOUSE_INPUT_FILTER_H_
#define REMOTING_PROTOCOL_MOUSE_INPUT_FILTER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "remoting/protocol/input_filter.h"
#include "remoting/protocol/input_stub.h"
#include "third_party/skia/include/core/SkTypes.h"
#include "third_party/skia/include/core/SkSize.h"

namespace remoting {
namespace protocol {

// Filtering InputStub implementation which scales mouse events based on the
// supplied input and output dimensions, and clamps their coordinates to the
// output dimensions, before passing events on to |input_stub|.
class MouseInputFilter : public InputFilter {
 public:
  explicit MouseInputFilter(InputStub* input_stub);
  virtual ~MouseInputFilter();

  // Specify the input dimensions for mouse events.
  void set_input_size(const SkISize& size);

  // Specify the output dimensions.
  void set_output_size(const SkISize& size);

  // InputStub overrides.
  virtual void InjectMouseEvent(const protocol::MouseEvent& event) OVERRIDE;

 private:
  SkISize input_max_;
  SkISize output_max_;

  DISALLOW_COPY_AND_ASSIGN(MouseInputFilter);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_MOUSE_INPUT_FILTER_H_
