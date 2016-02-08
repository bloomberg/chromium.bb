// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SECURITY_KEY_GNUBBY_AUTH_HANDLER_H_
#define REMOTING_HOST_SECURITY_KEY_GNUBBY_AUTH_HANDLER_H_

#include <string>

#include "base/memory/scoped_ptr.h"

namespace base {
class FilePath;
}  // namespace base

namespace remoting {

namespace protocol {
class ClientStub;
}  // namespace protocol

// Class responsible for proxying authentication data between a local gnubbyd
// and the client.
class GnubbyAuthHandler {
 public:
  virtual ~GnubbyAuthHandler() {}

  // Creates a platform-specific GnubbyAuthHandler.
  static scoped_ptr<GnubbyAuthHandler> Create(
      protocol::ClientStub* client_stub);

  // Specify the name of the socket to listen to gnubby requests on.
  static void SetGnubbySocketName(const base::FilePath& gnubby_socket_name);

  // A message was received from the client.
  virtual void DeliverClientMessage(const std::string& message) = 0;

  // Send data to client.
  virtual void DeliverHostDataMessage(int connection_id,
                                      const std::string& data) const = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_SECURITY_KEY_GNUBBY_AUTH_HANDLER_H_
