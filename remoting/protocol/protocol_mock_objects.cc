// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/protocol_mock_objects.h"

#include "base/message_loop_proxy.h"

namespace remoting {
namespace protocol {

MockConnectionToClient::MockConnectionToClient(
    EventHandler* handler,
    HostStub* host_stub,
    InputStub* input_stub)
    : ConnectionToClient(base::MessageLoopProxy::CreateForCurrentThread(),
                         handler) {
  set_host_stub(host_stub);
  set_input_stub(input_stub);
}

MockConnectionToClient::~MockConnectionToClient() {}

MockConnectionToClientEventHandler::MockConnectionToClientEventHandler() {}

MockConnectionToClientEventHandler::~MockConnectionToClientEventHandler() {}

MockInputStub::MockInputStub() {}

MockInputStub::~MockInputStub() {}

MockHostStub::MockHostStub() {}

MockHostStub::~MockHostStub() {}

MockClientStub::MockClientStub() {}

MockClientStub::~MockClientStub() {}

MockVideoStub::MockVideoStub() {}

MockVideoStub::~MockVideoStub() {}

MockSession::MockSession() {}

MockSession::~MockSession() {}

}  // namespace protocol
}  // namespace remoting
