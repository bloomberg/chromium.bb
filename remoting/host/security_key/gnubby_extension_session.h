// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SECURITY_KEY_GNUBBY_EXTENSION_SESSION_H_
#define REMOTING_HOST_SECURITY_KEY_GNUBBY_EXTENSION_SESSION_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "remoting/host/host_extension_session.h"

namespace base {
class DictionaryValue;
}

namespace remoting {

class ClientSessionDetails;
class GnubbyAuthHandler;

namespace protocol {
class ClientStub;
}

// A HostExtensionSession implementation that enables Security Key support.
class GnubbyExtensionSession : public HostExtensionSession {
 public:
  GnubbyExtensionSession(ClientSessionDetails* client_session_details,
                         protocol::ClientStub* client_stub);
  ~GnubbyExtensionSession() override;

  // HostExtensionSession interface.
  bool OnExtensionMessage(ClientSessionDetails* client_session_details,
                          protocol::ClientStub* client_stub,
                          const protocol::ExtensionMessage& message) override;

  // Allows the underlying GnubbyAuthHandler to be overridden for unit testing.
  void SetGnubbyAuthHandlerForTesting(
      std::unique_ptr<GnubbyAuthHandler> gnubby_auth_handler);

 private:
  // These methods process specific gnubby extension message types.
  void ProcessControlMessage(base::DictionaryValue* message_data) const;
  void ProcessDataMessage(base::DictionaryValue* message_data) const;
  void ProcessErrorMessage(base::DictionaryValue* message_data) const;

  void SendMessageToClient(int connection_id, const std::string& data) const;

  // Ensures GnubbyExtensionSession methods are called on the same thread.
  base::ThreadChecker thread_checker_;

  // Interface through which messages can be sent to the client.
  protocol::ClientStub* client_stub_ = nullptr;

  // Handles platform specific gnubby operations.
  std::unique_ptr<GnubbyAuthHandler> gnubby_auth_handler_;

  DISALLOW_COPY_AND_ASSIGN(GnubbyExtensionSession);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SECURITY_KEY_GNUBBY_EXTENSION_SESSION_H_
