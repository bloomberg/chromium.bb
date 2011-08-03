// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_JINGLE_GLUE_SIGNAL_STRATEGY_H_
#define REMOTING_JINGLE_GLUE_SIGNAL_STRATEGY_H_

#include <string>

#include "base/basictypes.h"

namespace buzz {
class XmlElement;
}  // namespace buzz

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

  class Listener {
   public:
    // Must return true if the stanza was handled, false otherwise.
    virtual bool OnIncomingStanza(const buzz::XmlElement* stanza) = 0;
  };

  SignalStrategy() {}
  virtual ~SignalStrategy() {}
  virtual void Init(StatusObserver* observer) = 0;
  virtual void Close() = 0;

  // Set a listener that can listen to all incoming messages. Doesn't
  // take ownership of the |listener|. Can be called with |listener|
  // set to NULL to unset current listener. It must be unset before
  // object is destroyed.
  virtual void SetListener(Listener* listener) = 0;

  // Sends a raw XMPP stanza. Takes ownership of the |stanza|.
  virtual void SendStanza(buzz::XmlElement* stanza) = 0;

  // Returns new ID that should be used for the next outgoing IQ
  // request.
  virtual std::string GetNextId() = 0;

  // TODO(sergeyu): Do we really need this method to be part of this
  // interface?
  virtual IqRequest* CreateIqRequest() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SignalStrategy);
};

}  // namespace remoting

#endif  // REMOTING_JINGLE_GLUE_SIGNAL_STRATEGY_H_
