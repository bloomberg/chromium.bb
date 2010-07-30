// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_JINGLE_GLUE_IQ_REQUEST_H_
#define REMOTING_JINGLE_GLUE_IQ_REQUEST_H_

#include <string>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "third_party/libjingle/source/talk/xmpp/xmppengine.h"

namespace remoting {

class JingleClient;

// IqRequest class can be used to send an IQ stanza and then receive reply
// stanza for that request. It sends outgoing stanza when SendIq() is called,
// after that it forwards incoming reply stanza to the callback set with
// set_callback(). If multiple IQ stanzas are send with SendIq() then only reply
// to the last one will be received.
// The class must be used on the jingle thread only.
// TODO(sergeyu): Implement unittests for this class.
class IqRequest : private buzz::XmppIqHandler {
 public:
  typedef Callback1<const buzz::XmlElement*>::Type ReplyCallback;

  explicit IqRequest(JingleClient* jingle_client);
  virtual ~IqRequest();

  // Sends stanza of type |type| to |addressee|. |iq_body| contains body of
  // the stanza. Ownership of |iq_body| is transfered to IqRequest. Must
  // be called on the jingle thread.
  void SendIq(const std::string& type, const std::string& addressee,
              buzz::XmlElement* iq_body);

  // Sets callback that is called when reply stanza is received. Callback
  // is called on the jingle thread.
  void set_callback(ReplyCallback* callback) {
    callback_.reset(callback);
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(IqRequestTest, MakeIqStanza);

  // XmppIqHandler interface.
  virtual void IqResponse(buzz::XmppIqCookie cookie,
                          const buzz::XmlElement* stanza);

  static buzz::XmlElement* MakeIqStanza(const std::string& type,
                                        const std::string& addressee,
                                        buzz::XmlElement* iq_body,
                                        const std::string& id);

  void Unregister();

  scoped_refptr<JingleClient> jingle_client_;
  buzz::XmppIqCookie cookie_;
  scoped_ptr<ReplyCallback> callback_;
};

}  // namespace remoting

#endif  // REMOTING_JINGLE_GLUE_IQ_REQUEST_H_
