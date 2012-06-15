// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/protocol_mock_objects.h"

#include "base/message_loop_proxy.h"
#include "net/base/ip_endpoint.h"
#include "remoting/protocol/transport.h"

namespace remoting {
namespace protocol {

MockConnectionToClient::MockConnectionToClient(
    Session* session,
    HostStub* host_stub,
    InputStub* input_stub)
    : ConnectionToClient(session) {
  set_host_stub(host_stub);
  set_input_stub(input_stub);
}

MockConnectionToClient::~MockConnectionToClient() {}

MockConnectionToClientEventHandler::MockConnectionToClientEventHandler() {}

MockConnectionToClientEventHandler::~MockConnectionToClientEventHandler() {}

MockClipboardStub::MockClipboardStub() {}

MockClipboardStub::~MockClipboardStub() {}

MockInputStub::MockInputStub() {}

MockInputStub::~MockInputStub() {}

MockHostEventStub::MockHostEventStub() {}

MockHostEventStub::~MockHostEventStub() {}

MockHostStub::MockHostStub() {}

MockHostStub::~MockHostStub() {}

MockClientStub::MockClientStub() {}

MockClientStub::~MockClientStub() {}

MockVideoStub::MockVideoStub() {}

MockVideoStub::~MockVideoStub() {}

MockSession::MockSession() {}

MockSession::~MockSession() {}

MockSessionManager::MockSessionManager() {}

MockSessionManager::~MockSessionManager() {}

}  // namespace protocol
}  // namespace remoting
