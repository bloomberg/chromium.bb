// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_JINGLE_GLUE_SIGNAL_STRATEGY_H_
#define REMOTING_JINGLE_GLUE_SIGNAL_STRATEGY_H_

#include <string>

#include "base/basictypes.h"

namespace cricket {
class SessionManager;
}  // namespace cricket

namespace remoting {

class IqRequest;

class SignalStrategy {
 public:
  class StatusObserver {
   public:
    enum State {
      START,
      CONNECTING,
      CONNECTED,
      CLOSED,
    };

    // Called when state of the connection is changed.
    virtual void OnStateChange(State state) = 0;
    virtual void OnJidChange(const std::string& full_jid) = 0;
  };

  SignalStrategy() {}
  virtual ~SignalStrategy() {}
  virtual void Init(StatusObserver* observer) = 0;

  // TODO(sergeyu): Do these methods belong to this interface?
  virtual void StartSession(cricket::SessionManager* session_manager) = 0;
  virtual void EndSession() = 0;

  virtual IqRequest* CreateIqRequest() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SignalStrategy);
};

}  // namespace remoting

#endif  // REMOTING_JINGLE_GLUE_SIGNAL_STRATEGY_H_
