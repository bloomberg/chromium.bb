// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_JINGLE_GLUE_JINGLE_SIGNALING_CONNECTOR_H_
#define REMOTING_JINGLE_GLUE_JINGLE_SIGNALING_CONNECTOR_H_

#include <map>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/jingle_glue/signal_strategy.h"
#include "third_party/libjingle/source/talk/base/sigslot.h"

namespace buzz {
class XmlElement;
}  // namespace buzz

namespace cricket {
class SessionManager;
}  // namespace cricket

namespace remoting {

class JavascriptIqRequest;

// This class handles proxying the Jingle establishment messages between the
// client and the server when proxying XMPP through Javascript.
//
// The |request| object is used to send and receive IQ stanzas from the XMPP
// network.  The |session_manager| controls sending and receiving of stanzas
// after Run() is invoked.
//
// This class is not threadsafe, and should only be used on the thread it is
// created on.
//
// TODO(sergeyu): This class should not depend on JavascriptIqRequest:
// it should work with SignalStrategy instead.
class JingleSignalingConnector : public SignalStrategy::Listener,
                                 public sigslot::has_slots<> {
 public:
  JingleSignalingConnector(SignalStrategy* signal_strategy,
                           cricket::SessionManager* session_manager);
  virtual ~JingleSignalingConnector();

  // SignalStrategy::Listener interface.
  virtual bool OnIncomingStanza(const buzz::XmlElement* stanza) OVERRIDE;

 private:
  typedef std::map<std::string, buzz::XmlElement*> IqRequestsMap;

  void OnOutgoingMessage(cricket::SessionManager* manager,
                         const buzz::XmlElement* stanza);

  SignalStrategy* signal_strategy_;
  cricket::SessionManager* session_manager_;
  IqRequestsMap pending_requests_;

  DISALLOW_COPY_AND_ASSIGN(JingleSignalingConnector);
};

}  // namespace remoting

#endif  // REMOTING_JINGLE_GLUE_JINGLE_SIGNALING_CONNECTOR_H_
