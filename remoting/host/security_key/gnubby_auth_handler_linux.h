// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SECURITY_KEY_GNUBBY_AUTH_HANDLER_LINUX_H_
#define REMOTING_HOST_SECURITY_KEY_GNUBBY_AUTH_HANDLER_LINUX_H_

#include <stddef.h>

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/completion_callback.h"
#include "net/socket/stream_socket.h"
#include "remoting/host/security_key/gnubby_auth_handler.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace net {
class UnixDomainServerSocket;
}  // namespace net

namespace remoting {

namespace protocol {
class ClientStub;
}  // namespace protocol

class GnubbySocket;

class GnubbyAuthHandlerLinux : public GnubbyAuthHandler,
                               public base::NonThreadSafe {
 public:
  explicit GnubbyAuthHandlerLinux(protocol::ClientStub* client_stub);
  ~GnubbyAuthHandlerLinux() override;

  size_t GetActiveSocketsMapSizeForTest() const;

  void SetRequestTimeoutForTest(const base::TimeDelta& timeout);

 private:
  typedef std::map<int, GnubbySocket*> ActiveSockets;

  // GnubbyAuthHandler interface.
  void DeliverClientMessage(const std::string& message) override;
  void DeliverHostDataMessage(int connection_id,
                              const std::string& data) const override;

  // Starts listening for connection.
  void DoAccept();

  // Called when a connection is accepted.
  void OnAccepted(int result);

  // Called when a GnubbySocket has done reading.
  void OnReadComplete(int connection_id);

  // Create socket for authorization.
  void CreateAuthorizationSocket();

  // Process a gnubby request.
  void ProcessGnubbyRequest(int connection_id, const std::string& request_data);

  // Gets an active socket iterator for the connection id in |message|.
  ActiveSockets::iterator GetSocketForMessage(base::DictionaryValue* message);

  // Send an error and closes an active socket.
  void SendErrorAndCloseActiveSocket(const ActiveSockets::iterator& iter);

  // A request timed out.
  void RequestTimedOut(int connection_id);

  // Interface through which communication with the client occurs.
  protocol::ClientStub* client_stub_;

  // Socket used to listen for authorization requests.
  scoped_ptr<net::UnixDomainServerSocket> auth_socket_;

  // A temporary holder for an accepted connection.
  scoped_ptr<net::StreamSocket> accept_socket_;

  // The last assigned gnubby connection id.
  int last_connection_id_;

  // Sockets by connection id used to process gnubbyd requests.
  ActiveSockets active_sockets_;

  // Timeout used for a request.
  base::TimeDelta request_timeout_;

  DISALLOW_COPY_AND_ASSIGN(GnubbyAuthHandlerLinux);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SECURITY_KEY_GNUBBY_AUTH_HANDLER_LINUX_H_
