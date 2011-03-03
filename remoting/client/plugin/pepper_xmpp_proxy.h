// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_PEPPER_XMPP_PROXY_H_
#define REMOTING_CLIENT_PLUGIN_PEPPER_XMPP_PROXY_H_

#include "base/weak_ptr.h"
#include "remoting/jingle_glue/iq_request.h"

namespace remoting {

class PepperXmppProxy : public XmppProxy {
 public:
  PepperXmppProxy(MessageLoop* jingle_message_loop,
                  MessageLoop* pepper_message_loop);

  // Must be run on pepper thread.
  void AttachScriptableObject(ChromotingScriptableObject* scriptable_object);

  // Must be run on jingle thread.
  void AttachJavascriptIqRequest(JavascriptIqRequest* javascript_iq_request);

  void SendIq(const std::string& iq_request_xml);
  void ReceiveIq(const std::string& iq_response_xml);

 private:
  ~PepperXmppProxy();

  MessageLoop* jingle_message_loop_;
  MessageLoop* pepper_message_loop_;

  base::WeakPtr<ChromotingScriptableObject> scriptable_object_;
  base::WeakPtr<JavascriptIqRequest> javascript_iq_request_;
  std::string iq_response_xml_;

  DISALLOW_COPY_AND_ASSIGN(PepperXmppProxy);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_PEPPER_XMPP_PROXY_H_
