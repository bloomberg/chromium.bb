// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_JINGLE_GLUE_JAVASCRIPT_IQ_REQUEST_H_
#define REMOTING_JINGLE_GLUE_JAVASCRIPT_IQ_REQUEST_H_

#include <map>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/jingle_glue/iq_request.h"
#include "remoting/jingle_glue/xmpp_proxy.h"

namespace remoting {

class JavascriptIqRequest;

class JavascriptIqRegistry : public XmppProxy::ResponseCallback {
 public:
  JavascriptIqRegistry();
  virtual ~JavascriptIqRegistry();

  // Dispatches the response to the IqRequest callback immediately.
  //
  // Does not take ownership of stanza.
  void DispatchResponse(buzz::XmlElement* stanza);

  // Registers |request|, returning the request ID used.
  std::string RegisterRequest(JavascriptIqRequest* request);

  // Removes all entries in the registry that refer to |request|.  Useful when
  // |request| is about to be destructed.
  void RemoveAllRequests(JavascriptIqRequest* request);

  void SetDefaultHandler(JavascriptIqRequest* default_request);

 private:
  typedef std::map<std::string, JavascriptIqRequest*> IqRequestMap;

  // XmppProxy::ResponseCallback interface.
  virtual void OnIq(const std::string& response_xml);

  IqRequestMap requests_;
  int current_id_;
  JavascriptIqRequest* default_handler_;

  DISALLOW_COPY_AND_ASSIGN(JavascriptIqRegistry);
};

// This call must only be used on the thread it was created on.
class JavascriptIqRequest : public IqRequest {
 public:
  JavascriptIqRequest(JavascriptIqRegistry* registry,
                      scoped_refptr<XmppProxy> xmpp_proxy);
  virtual ~JavascriptIqRequest();

  // Similar to SendIq(), but has 3 major differences:
  //
  //   (1) It does absoluately no error checking. Caller is responsible for
  //       validity.
  //   (2) It doesn't add an Iq envelope. Caller is again responsible.
  //   (3) BecomeDefaultHandler() must have been called.
  //
  // TODO(ajwong): We need to rationalize the semantics of these two
  // APIs. SendRawIq() is a hack for JingleSignalingConnector which
  // uses a different memory management convention.
  void SendRawIq(buzz::XmlElement* stanza);

  // This function is a hack to support SessionStartRequest. It registers the
  // current JavascriptIqRequest instance to be the single passthrough filter
  // for the associated JavascriptIqRegistry.  What this means is that any IQ
  // packet that does not match a previous respone packet will be funneled
  // through to this JavascriptIqRegistry instance (basically a default
  // handler).  It also means that the registry will not be tracking any of the
  // packets sent from this JavascriptIqRegistry instance.
  //
  // TODO(ajwong): We need to take a high-level look at IqRequest to understand
  // how to make this API cleaner.
  void BecomeDefaultHandler();

  //  IqRequest interface.
  virtual void SendIq(const std::string& type, const std::string& addressee,
                      buzz::XmlElement* iq_body) OVERRIDE;
  virtual void set_callback(ReplyCallback* callback) OVERRIDE;

 private:
  friend class JavascriptIqRegistry;

  scoped_ptr<ReplyCallback> callback_;
  scoped_refptr<XmppProxy> xmpp_proxy_;
  JavascriptIqRegistry* registry_;
  bool is_default_handler_;

  FRIEND_TEST_ALL_PREFIXES(IqRequestTest, MakeIqStanza);
};

}  // namespace remoting

#endif  // REMOTING_JINGLE_GLUE_JAVASCRIPT_IQ_REQUEST_H_
