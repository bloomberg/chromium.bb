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
#include "base/threading/thread_checker.h"
#include "net/base/completion_callback.h"
#include "net/socket/stream_socket.h"
#include "remoting/host/security_key/gnubby_auth_handler.h"

namespace net {
class UnixDomainServerSocket;
}  // namespace net

namespace remoting {

class GnubbySocket;

class GnubbyAuthHandlerLinux : public GnubbyAuthHandler {
 public:
  GnubbyAuthHandlerLinux();
  ~GnubbyAuthHandlerLinux() override;

  size_t GetActiveSocketsMapSizeForTest() const;

  void SetRequestTimeoutForTest(const base::TimeDelta& timeout);

 private:
  typedef std::map<int, GnubbySocket*> ActiveSockets;

  // GnubbyAuthHandler interface.
  void CreateGnubbyConnection() override;
  bool IsValidConnectionId(int gnubby_connection_id) const override;
  void SendClientResponse(int gnubby_connection_id,
                          const std::string& response) override;
  void SendErrorAndCloseConnection(int gnubby_connection_id) override;
  void SetSendMessageCallback(const SendMessageCallback& callback) override;

  // Starts listening for connection.
  void DoAccept();

  // Called when a connection is accepted.
  void OnAccepted(int result);

  // Called when a GnubbySocket has done reading.
  void OnReadComplete(int gnubby_connection_id);

  // Gets an active socket iterator for |gnubby_connection_id|.
  ActiveSockets::const_iterator GetSocketForConnectionId(
      int gnubby_connection_id) const;

  // Send an error and closes an active socket.
  void SendErrorAndCloseActiveSocket(const ActiveSockets::const_iterator& iter);

  // A request timed out.
  void RequestTimedOut(int gnubby_connection_id);

  // Ensures GnubbyAuthHandlerLinux methods are called on the same thread.
  base::ThreadChecker thread_checker_;

  // Socket used to listen for authorization requests.
  scoped_ptr<net::UnixDomainServerSocket> auth_socket_;

  // A temporary holder for an accepted connection.
  scoped_ptr<net::StreamSocket> accept_socket_;

  // Used to pass gnubby extension messages to the client.
  SendMessageCallback send_message_callback_;

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
