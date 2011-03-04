// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/protocol_mock_objects.h"

namespace remoting {
namespace protocol {

MockConnectionToClient::MockConnectionToClient(MessageLoop* message_loop,
                                               EventHandler* handler,
                                               HostStub* host_stub,
                                               InputStub* input_stub)
    : ConnectionToClient(message_loop, handler, host_stub, input_stub) {
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
