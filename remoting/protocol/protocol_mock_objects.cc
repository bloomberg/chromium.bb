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

void MockPairingRegistryDelegate::AddPairing(
    const PairingRegistry::Pairing& new_paired_client,
    const PairingRegistry::AddPairingCallback& callback) {
  paired_clients_[new_paired_client.client_id()] = new_paired_client;
  if (!callback.is_null()) {
    callback.Run(true);
  }
}

void MockPairingRegistryDelegate::GetPairing(
    const std::string& client_id,
    const PairingRegistry::GetPairingCallback& callback) {
  PairingRegistry::Pairing result;
  PairingRegistry::PairedClients::const_iterator i =
      paired_clients_.find(client_id);
  if (i != paired_clients_.end()) {
    result = i->second;
  }
  saved_callback_ = base::Bind(base::Bind(callback), result);
}

void MockPairingRegistryDelegate::RunCallback() {
  DCHECK(!saved_callback_.is_null());
  saved_callback_.Run();
  saved_callback_.Reset();
}

}  // namespace protocol
}  // namespace remoting
