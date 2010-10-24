// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CHROMOTING_SERVER_H_
#define REMOTING_PROTOCOL_CHROMOTING_SERVER_H_

#include <string>

#include "base/callback.h"
#include "base/ref_counted.h"
#include "remoting/protocol/chromoting_connection.h"

class Task;

namespace remoting {

// Generic interface for Chromoting server.
// TODO(sergeyu): Rename to ChromotocolServer.
// TODO(sergeyu): Add more documentation on how this interface is used.
class ChromotingServer : public base::RefCountedThreadSafe<ChromotingServer> {
 public:
  enum NewConnectionResponse {
    ACCEPT,
    INCOMPATIBLE,
    DECLINE,
  };

  // NewConnectionCallback is called when a new connection is received. If the
  // callback decides to accept the connection it should set the second
  // argument to ACCEPT. Otherwise it should set it to DECLINE, or
  // INCOMPATIBLE. INCOMPATIBLE indicates that the session has incompartible
  // configuration, and cannot be accepted.
  // If the callback accepts connection then it must also set configuration
  // for the new connection using ChromotingConnection::set_config().
  typedef Callback2<ChromotingConnection*, NewConnectionResponse*>::Type
      NewConnectionCallback;

  // Initializes connection to the host |jid|.  Ownership of the
  // |chromotocol_config| is passed to the new connection.
  virtual scoped_refptr<ChromotingConnection> Connect(
      const std::string& jid,
      CandidateChromotocolConfig* chromotocol_config,
      ChromotingConnection::StateChangeCallback* state_change_callback) = 0;

  // Close connection. |close_task| is executed after the session client
  // is actually closed. No callbacks are called after |closed_task| is
  // executed.
  virtual void Close(Task* closed_task) = 0;

 protected:
  friend class base::RefCountedThreadSafe<ChromotingServer>;
  virtual ~ChromotingServer() { }
};

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CHROMOTING_SERVER_H_
