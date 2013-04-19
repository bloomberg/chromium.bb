// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_CLIENT_USER_INTERFACE_H_
#define REMOTING_CLIENT_CLIENT_USER_INTERFACE_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/protocol/connection_to_host.h"
#include "remoting/protocol/third_party_client_authenticator.h"

namespace remoting {

namespace protocol {
class ClipboardStub;
class CursorShapeStub;
}  // namespace protocol

// ClientUserInterface is an interface that must be implemented by
// applications embedding the Chromoting client, to provide client's user
// interface.
//
// TODO(sergeyu): Cleanup this interface, see crbug.com/138108 .
class ClientUserInterface {
 public:
  virtual ~ClientUserInterface() {}

  // Record the update the state of the connection, updating the UI as needed.
  virtual void OnConnectionState(protocol::ConnectionToHost::State state,
                                 protocol::ErrorCode error) = 0;
  virtual void OnConnectionReady(bool ready) = 0;

  // Passes the final set of capabilities negotiated between the client and host
  // to the application.
  virtual void SetCapabilities(const std::string& capabilities) = 0;

  // Get the view's ClipboardStub implementation.
  virtual protocol::ClipboardStub* GetClipboardStub() = 0;

  // Get the view's CursorShapeStub implementation.
  virtual protocol::CursorShapeStub* GetCursorShapeStub() = 0;

  // Get the view's TokenFetcher implementation.
  // The TokenFetcher implementation may require interactive authentication.
  virtual scoped_ptr<protocol::ThirdPartyClientAuthenticator::TokenFetcher>
  GetTokenFetcher(const std::string& host_public_key) = 0;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_CLIENT_USER_INTERFACE_H_
