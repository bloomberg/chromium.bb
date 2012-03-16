// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Interface for an object that handles input and clipboard events.

#ifndef REMOTING_PROTOCOL_HOST_EVENT_STUB_H_
#define REMOTING_PROTOCOL_HOST_EVENT_STUB_H_

#include "base/basictypes.h"
#include "remoting/protocol/clipboard_stub.h"
#include "remoting/protocol/input_stub.h"

namespace remoting {
namespace protocol {

class HostEventStub : public ClipboardStub, public InputStub {
 public:
  HostEventStub() {}
  virtual ~HostEventStub() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(HostEventStub);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_HOST_EVENT_STUB_H_
