// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_INPUT_FILTER_H_
#define REMOTING_PROTOCOL_INPUT_FILTER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "remoting/protocol/input_stub.h"

namespace remoting {
namespace protocol {

// Forwards input events to |input_stub|, iif |input_stub| is not NULL.
class InputFilter : public InputStub {
 public:
  InputFilter();
  explicit InputFilter(InputStub* input_stub);
  virtual ~InputFilter();

  // Return the InputStub that events will be forwarded to.
  InputStub* input_stub() { return input_stub_; }

  // Set the InputStub that events will be forwarded to.
  void set_input_stub(InputStub* input_stub) {
    input_stub_ = input_stub;
  }

  // InputStub interface.
  virtual void InjectKeyEvent(const KeyEvent& event) OVERRIDE;
  virtual void InjectMouseEvent(const MouseEvent& event) OVERRIDE;

 private:
  InputStub* input_stub_;

  DISALLOW_COPY_AND_ASSIGN(InputFilter);
};

} // namespace protocol
} // namespace remoting

#endif // REMOTING_PROTOCOL_INPUT_FILTER_H_
