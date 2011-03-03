// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_JINGLE_GLUE_IQ_REQUEST_H_
#define REMOTING_JINGLE_GLUE_IQ_REQUEST_H_

#include <string>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/weak_ptr.h"
#include "third_party/libjingle/source/talk/xmpp/xmppengine.h"

class MessageLoop;

namespace buzz {
class XmppClient;
}  // namespace buzz

namespace remoting {

class JavascriptIqRequest;
class ChromotingScriptableObject;

class XmppProxy : public base::RefCountedThreadSafe<XmppProxy> {
 public:
  XmppProxy() {}

  // Must be run on pepper thread.
  virtual void AttachScriptableObject(
      ChromotingScriptableObject* scriptable_object) = 0;

  // Must be run on jingle thread.
  virtual void AttachJavascriptIqRequest(
      JavascriptIqRequest* javascript_iq_request) = 0;

  virtual void SendIq(const std::string& iq_request_xml) = 0;
  virtual void ReceiveIq(const std::string& iq_response_xml) = 0;

 protected:
  friend class base::RefCountedThreadSafe<XmppProxy>;
  virtual ~XmppProxy() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(XmppProxy);
};

// IqRequest class can be used to send an IQ stanza and then receive reply
// stanza for that request. It sends outgoing stanza when SendIq() is called,
// after that it forwards incoming reply stanza to the callback set with
// set_callback(). If multiple IQ stanzas are send with SendIq() then only reply
// to the last one will be received.
// The class must be used on the jingle thread only.
class IqRequest {
 public:
  typedef Callback1<const buzz::XmlElement*>::Type ReplyCallback;

  IqRequest() {}
  virtual ~IqRequest() {}

  // Sends stanza of type |type| to |addressee|. |iq_body| contains body of
  // the stanza. Ownership of |iq_body| is transfered to IqRequest. Must
  // be called on the jingle thread.
  virtual void SendIq(const std::string& type, const std::string& addressee,
                      buzz::XmlElement* iq_body) = 0;

  // Sets callback that is called when reply stanza is received. Callback
  // is called on the jingle thread.
  virtual void set_callback(ReplyCallback* callback) = 0;

 protected:
  static buzz::XmlElement* MakeIqStanza(const std::string& type,
                                        const std::string& addressee,
                                        buzz::XmlElement* iq_body,
                                        const std::string& id);

 private:
  FRIEND_TEST_ALL_PREFIXES(IqRequestTest, MakeIqStanza);

  DISALLOW_COPY_AND_ASSIGN(IqRequest);
};

// TODO(ajwong): Is this class even used? The client side may never use
// IqRequests in the JingleClient.
class JavascriptIqRequest : public IqRequest,
                            public base::SupportsWeakPtr<JavascriptIqRequest> {
 public:
  JavascriptIqRequest();
  virtual ~JavascriptIqRequest();

  virtual void SendIq(const std::string& type, const std::string& addressee,
                      buzz::XmlElement* iq_body);

  virtual void ReceiveIq(const std::string& iq_response);

  virtual void set_callback(ReplyCallback* callback);

 private:
  scoped_ptr<ReplyCallback> callback_;
  scoped_refptr<XmppProxy> xmpp_proxy_;

  FRIEND_TEST_ALL_PREFIXES(IqRequestTest, MakeIqStanza);
};

class XmppIqRequest : public IqRequest, private buzz::XmppIqHandler {
 public:
  typedef Callback1<const buzz::XmlElement*>::Type ReplyCallback;

  XmppIqRequest(MessageLoop* message_loop, buzz::XmppClient* xmpp_client);
  virtual ~XmppIqRequest();

  virtual void SendIq(const std::string& type, const std::string& addressee,
                      buzz::XmlElement* iq_body);
  virtual void set_callback(ReplyCallback* callback) {
    callback_.reset(callback);
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(IqRequestTest, MakeIqStanza);

  // XmppIqHandler interface.
  virtual void IqResponse(buzz::XmppIqCookie cookie,
                          const buzz::XmlElement* stanza);

  void Unregister();

  // TODO(ajwong): This used to hold a reference to the jingle client...make
  // sure the lifetime names sense now.
  MessageLoop* message_loop_;
  buzz::XmppClient* xmpp_client_;
  buzz::XmppIqCookie cookie_;
  scoped_ptr<ReplyCallback> callback_;
};

}  // namespace remoting

#endif  // REMOTING_JINGLE_GLUE_IQ_REQUEST_H_
