// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Interface of a client that receives commands from a Chromoting host.
//
// This interface is responsible for a subset of control messages sent to
// the Chromoting client.

#ifndef REMOTING_PROTOCOL_CLIENT_STUB_H_
#define REMOTING_PROTOCOL_CLIENT_STUB_H_

#include "base/basictypes.h"

namespace remoting {
namespace protocol {

class ClientStub {
 public:
  ClientStub() {}
  virtual ~ClientStub() {}

  // Currently we don't use the control channel for anything. Add new
  // message handlers here when necessary.

 private:
  DISALLOW_COPY_AND_ASSIGN(ClientStub);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CLIENT_STUB_H_
