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

MockPairingRegistryDelegate::MockPairingRegistryDelegate()
    : run_save_callback_automatically_(true) {
}

MockPairingRegistryDelegate::~MockPairingRegistryDelegate() {
}

void MockPairingRegistryDelegate::Save(
    const std::string& pairings_json,
    const PairingRegistry::SaveCallback& callback) {
  EXPECT_TRUE(load_callback_.is_null());
  EXPECT_TRUE(save_callback_.is_null());
  if (run_save_callback_automatically_) {
    SetPairingsJsonAndRunCallback(pairings_json, callback);
  } else {
    save_callback_ = base::Bind(
        &MockPairingRegistryDelegate::SetPairingsJsonAndRunCallback,
        base::Unretained(this), pairings_json, callback);
  }
}

void MockPairingRegistryDelegate::SetPairingsJsonAndRunCallback(
    const std::string& pairings_json,
    const PairingRegistry::SaveCallback& callback) {
  pairings_json_ = pairings_json;
  if (!callback.is_null()) {
    callback.Run(true);
  }
}

void MockPairingRegistryDelegate::Load(
    const PairingRegistry::LoadCallback& callback) {
  EXPECT_TRUE(load_callback_.is_null());
  EXPECT_TRUE(save_callback_.is_null());
  load_callback_ = base::Bind(callback, pairings_json_);
}

void MockPairingRegistryDelegate::RunCallback() {
  if (!load_callback_.is_null()) {
    EXPECT_TRUE(save_callback_.is_null());
    base::Closure load_callback = load_callback_;
    load_callback_.Reset();
    load_callback.Run();
  } else if (!save_callback_.is_null()) {
    EXPECT_TRUE(load_callback_.is_null());
    base::Closure save_callback = save_callback_;
    save_callback_.Reset();
    save_callback.Run();
  } else {
    ADD_FAILURE() << "RunCallback called without any callbacks set.";
  }
}

}  // namespace protocol
}  // namespace remoting
