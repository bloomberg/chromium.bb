// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_AUTH_HANDLER_H_
#define REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_AUTH_HANDLER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"

namespace base {
class FilePath;
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

class ClientSessionDetails;

// Class responsible for proxying authentication data between a local gnubbyd
// and the client.
class SecurityKeyAuthHandler {
 public:
  virtual ~SecurityKeyAuthHandler() {}

  // Used to send security key extension messages to the client.
  typedef base::Callback<void(int connection_id, const std::string& data)>
      SendMessageCallback;

  // Creates a platform-specific SecurityKeyAuthHandler.
  // All invocations of |send_message_callback| are guaranteed to occur before
  // the underlying SecurityKeyAuthHandler object is destroyed.  It is not safe
  // to destroy the SecurityKeyAuthHandler object within the callback.
  // |client_session_details| will be valid until this instance is destroyed.
  static std::unique_ptr<SecurityKeyAuthHandler> Create(
      ClientSessionDetails* client_session_details,
      const SendMessageCallback& send_message_callback,
      scoped_refptr<base::SingleThreadTaskRunner> file_task_runner);

#if defined(OS_POSIX)
  // Specify the name of the socket to listen to security key requests on.
  static void SetSecurityKeySocketName(
      const base::FilePath& security_key_socket_name);
#endif  // defined(OS_POSIX)

  // Sets the callback used to send messages to the client.
  virtual void SetSendMessageCallback(const SendMessageCallback& callback) = 0;

  // Creates the platform specific connection to handle security key requests.
  virtual void CreateSecurityKeyConnection() = 0;

  // Returns true if |security_key_connection_id| represents a valid connection.
  virtual bool IsValidConnectionId(int security_key_connection_id) const = 0;

  // Sends security key response from client to local security key agent.
  virtual void SendClientResponse(int security_key_connection_id,
                                  const std::string& response) = 0;

  // Closes key connection represented by |security_key_connection_id|.
  virtual void SendErrorAndCloseConnection(int security_key_connection_id) = 0;

  // Returns the number of active security key connections.
  virtual size_t GetActiveConnectionCountForTest() const = 0;

  // Sets the timeout used when waiting for a security key response.
  virtual void SetRequestTimeoutForTest(base::TimeDelta timeout) = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_AUTH_HANDLER_H_
