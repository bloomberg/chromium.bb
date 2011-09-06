// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_JINGLE_GLUE_XMPP_IQ_REQUEST_H_
#define REMOTING_JINGLE_GLUE_XMPP_IQ_REQUEST_H_

#include "base/compiler_specific.h"
#include "remoting/jingle_glue/iq_request.h"
#include "third_party/libjingle/source/talk/base/sigslot.h"
#include "third_party/libjingle/source/talk/xmpp/xmppengine.h"

class MessageLoop;

namespace buzz {
class XmppClient;
}  // namespace buzz

namespace remoting {

class XmppIqRequest : public IqRequest, public buzz::XmppIqHandler {
 public:
  XmppIqRequest(MessageLoop* message_loop, buzz::XmppClient* xmpp_client);
  virtual ~XmppIqRequest();

  // IqRequest interface.
  virtual void SendIq(const std::string& type, const std::string& addressee,
                      buzz::XmlElement* iq_body) OVERRIDE;
  virtual void set_callback(const ReplyCallback& callback) OVERRIDE;

  // buzz::XmppIqHandler interface.
  virtual void IqResponse(buzz::XmppIqCookie cookie,
                          const buzz::XmlElement* stanza) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(IqRequestTest, MakeIqStanza);

  void Unregister();

  // TODO(ajwong): This used to hold a reference to the jingle client...make
  // sure the lifetime names sense now.
  MessageLoop* message_loop_;
  buzz::XmppClient* xmpp_client_;
  buzz::XmppIqCookie cookie_;
  ReplyCallback callback_;
};

}  // namespace remoting

#endif  // REMOTING_JINGLE_GLUE_XMPP_IQ_REQUEST_H_
