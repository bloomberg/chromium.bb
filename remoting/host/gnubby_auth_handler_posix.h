// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_GNUBBY_AUTH_HANDLER_POSIX_H_
#define REMOTING_HOST_GNUBBY_AUTH_HANDLER_POSIX_H_

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "net/socket/stream_listen_socket.h"
#include "remoting/host/gnubby_auth_handler.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace remoting {

namespace protocol {
class ClientStub;
}  // namespace protocol

class GnubbySocket;

class GnubbyAuthHandlerPosix : public GnubbyAuthHandler,
                               public base::NonThreadSafe,
                               public net::StreamListenSocket::Delegate {
 public:
  explicit GnubbyAuthHandlerPosix(protocol::ClientStub* client_stub);
  virtual ~GnubbyAuthHandlerPosix();

  bool HasActiveSocketForTesting(net::StreamListenSocket* socket) const;
  int GetConnectionIdForTesting(net::StreamListenSocket* socket) const;
  GnubbySocket* GetGnubbySocketForTesting(
      net::StreamListenSocket* socket) const;

 private:
  typedef std::map<int, GnubbySocket*> ActiveSockets;

  // GnubbyAuthHandler interface.
  virtual void DeliverClientMessage(const std::string& message) OVERRIDE;
  virtual void DeliverHostDataMessage(int connection_id,
                                      const std::string& data) const OVERRIDE;

  // StreamListenSocket::Delegate interface.
  virtual void DidAccept(net::StreamListenSocket* server,
                         scoped_ptr<net::StreamListenSocket> socket) OVERRIDE;
  virtual void DidRead(net::StreamListenSocket* socket,
                       const char* data,
                       int len) OVERRIDE;
  virtual void DidClose(net::StreamListenSocket* socket) OVERRIDE;

  // Create socket for authorization.
  void CreateAuthorizationSocket();

  // Process a gnubby request.
  void ProcessGnubbyRequest(int connection_id, const std::string& request_data);

  // Gets an active socket iterator for the connection id in |message|.
  ActiveSockets::iterator GetSocketForMessage(base::DictionaryValue* message);

  // Send an error and close an active socket.
  void SendErrorAndCloseActiveSocket(const ActiveSockets::iterator& iter);

  // A request timed out.
  void RequestTimedOut(int connection_id);

  // Interface through which communication with the client occurs.
  protocol::ClientStub* client_stub_;

  // Socket used to listen for authorization requests.
  scoped_ptr<net::StreamListenSocket> auth_socket_;

  // The last assigned gnubby connection id.
  int last_connection_id_;

  // Sockets by connection id used to process gnubbyd requests.
  ActiveSockets active_sockets_;

  DISALLOW_COPY_AND_ASSIGN(GnubbyAuthHandlerPosix);
};

}  // namespace remoting

#endif  // REMOTING_HOST_GNUBBY_AUTH_HANDLER_POSIX_H_
