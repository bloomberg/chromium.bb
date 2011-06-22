// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_JINGLE_GLUE_JINGLE_SIGNALING_CONNECTOR_H_
#define REMOTING_JINGLE_GLUE_JINGLE_SIGNALING_CONNECTOR_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
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
class JingleSignalingConnector : public sigslot::has_slots<> {
 public:
  JingleSignalingConnector(JavascriptIqRequest* request,
                            cricket::SessionManager* session_manager);
  virtual ~JingleSignalingConnector();

  void Run();

 private:
  void OnResponse(const buzz::XmlElement* stanza);

  void OnOutgoingMessage(cricket::SessionManager* manager,
                         const buzz::XmlElement* stanza);

  scoped_ptr<JavascriptIqRequest> request_;
  cricket::SessionManager* session_manager_;

  DISALLOW_COPY_AND_ASSIGN(JingleSignalingConnector);
};

}  // namespace remoting

#endif  // REMOTING_JINGLE_GLUE_JINGLE_SIGNALING_CONNECTOR_H_
