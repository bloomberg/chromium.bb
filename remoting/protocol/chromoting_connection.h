// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CHROMOTING_CONNECTION_H_
#define REMOTING_PROTOCOL_CHROMOTING_CONNECTION_H_

#include <string>

#include "base/callback.h"

class Task;

namespace net {
class Socket;
}  // namespace net

namespace remoting {

// Generic interface for Chromoting connection.
class ChromotingConnection
    : public base::RefCountedThreadSafe<ChromotingConnection> {
 public:
  enum State {
    INITIALIZING,
    CONNECTING,
    CONNECTED,
    CLOSED,
    FAILED,
  };

  typedef Callback1<State>::Type StateChangeCallback;

  // Set callback that is called when state of the connection is changed.
  // Must be called on the jingle thread only.
  virtual void SetStateChangeCallback(StateChangeCallback* callback) = 0;

  // Reliable PseudoTCP channels for this connection.
  // TODO(sergeyu): Remove VideoChannel, and use RTP channels instead.
  virtual net::Socket* GetVideoChannel() = 0;
  virtual net::Socket* GetEventsChannel() = 0;

  // Unreliable channels for this connection.
  virtual net::Socket* GetVideoRtpChannel() = 0;
  virtual net::Socket* GetVideoRtcpChannel() = 0;

  // JID of the other side.
  virtual const std::string& jid() = 0;

  // Closed connection. Callbacks are guaranteed not to be called after
  // |closed_task| is executed.
  virtual void Close(Task* closed_task) = 0;

 protected:
  friend class base::RefCountedThreadSafe<ChromotingConnection>;
  virtual ~ChromotingConnection() { }
};

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CHROMOTING_CONNECTION_H_
