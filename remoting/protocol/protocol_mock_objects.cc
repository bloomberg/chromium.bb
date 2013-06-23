// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/protocol_mock_objects.h"

namespace remoting {
namespace protocol {

MockConnectionToClient::MockConnectionToClient(
    Session* session,
    HostStub* host_stub)
    : ConnectionToClient(session) {
  set_host_stub(host_stub);
}

MockConnectionToClient::~MockConnectionToClient() {}

MockConnectionToClientEventHandler::MockConnectionToClientEventHandler() {}

MockConnectionToClientEventHandler::~MockConnectionToClientEventHandler() {}

MockClipboardStub::MockClipboardStub() {}

MockClipboardStub::~MockClipboardStub() {}

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

MockSessionManager::MockSessionManager() {}

MockSessionManager::~MockSessionManager() {}

MockPairingRegistryDelegate::MockPairingRegistryDelegate() {
}

MockPairingRegistryDelegate::~MockPairingRegistryDelegate() {
}

void MockPairingRegistryDelegate::Save(
    const std::string& pairings_json,
    const PairingRegistry::SaveCallback& callback) {
  pairings_json_ = pairings_json;
  if (!callback.is_null()) {
    callback.Run(true);
  }
}

void MockPairingRegistryDelegate::Load(
    const PairingRegistry::LoadCallback& callback) {
  load_callback_ = base::Bind(base::Bind(callback), pairings_json_);
}

void MockPairingRegistryDelegate::RunCallback() {
  DCHECK(!load_callback_.is_null());
  load_callback_.Run();
  load_callback_.Reset();
}

}  // namespace protocol
}  // namespace remoting
