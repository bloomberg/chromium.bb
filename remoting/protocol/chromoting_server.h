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
class ChromotingServer : public base::RefCountedThreadSafe<ChromotingServer> {
 public:
  typedef Callback2<ChromotingConnection*, bool*>::Type NewConnectionCallback;

  // Initializes connection to the host |jid|.
  virtual scoped_refptr<ChromotingConnection> Connect(
      const std::string& jid,
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
