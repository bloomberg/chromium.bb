// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SECURITY_KEY_GNUBBY_AUTH_HANDLER_H_
#define REMOTING_HOST_SECURITY_KEY_GNUBBY_AUTH_HANDLER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/time/time.h"

namespace base {
class FilePath;
}  // namespace base

namespace remoting {

class ClientSessionDetails;

// Class responsible for proxying authentication data between a local gnubbyd
// and the client.
class GnubbyAuthHandler {
 public:
  virtual ~GnubbyAuthHandler() {}

  // Used to send gnubby extension messages to the client.
  typedef base::Callback<void(int connection_id, const std::string& data)>
      SendMessageCallback;

  // Creates a platform-specific GnubbyAuthHandler.
  // All invocations of |send_message_callback| are guaranteed to occur before
  // the underlying GnubbyAuthHandler object is destroyed.  It is not safe to
  // destroy the GnubbyAuthHandler object within the callback.
  // |client_session_details| will be valid until this instance is destroyed.
  static std::unique_ptr<GnubbyAuthHandler> Create(
      ClientSessionDetails* client_session_details,
      const SendMessageCallback& send_message_callback);

#if defined(OS_LINUX)
  // Specify the name of the socket to listen to gnubby requests on.
  static void SetGnubbySocketName(const base::FilePath& gnubby_socket_name);
#endif  // defined(OS_LINUX)

  // Sets the callback used to send messages to the client.
  virtual void SetSendMessageCallback(const SendMessageCallback& callback) = 0;

  // Creates the platform specific connection to handle gnubby requests.
  virtual void CreateGnubbyConnection() = 0;

  // Returns true if |gnubby_connection_id| represents a valid connection.
  virtual bool IsValidConnectionId(int gnubby_connection_id) const = 0;

  // Sends the gnubby response from the client to the local gnubby agent.
  virtual void SendClientResponse(int gnubby_connection_id,
                                  const std::string& response) = 0;

  // Closes the gnubby connection represented by |gnubby_connection_id|.
  virtual void SendErrorAndCloseConnection(int gnubby_connection_id) = 0;

  // Returns the number of active gnubby connections.
  virtual size_t GetActiveConnectionCountForTest() const = 0;

  // Sets the timeout used when waiting for a gnubby response.
  virtual void SetRequestTimeoutForTest(base::TimeDelta timeout) = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_SECURITY_KEY_GNUBBY_AUTH_HANDLER_H_
