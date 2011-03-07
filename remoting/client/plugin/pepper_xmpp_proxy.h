// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_PEPPER_XMPP_PROXY_H_
#define REMOTING_CLIENT_PLUGIN_PEPPER_XMPP_PROXY_H_

#include "base/weak_ptr.h"
#include "remoting/jingle_glue/xmpp_proxy.h"

namespace remoting {

class ChromotingScriptableObject;

class PepperXmppProxy : public XmppProxy {
 public:
  PepperXmppProxy(
      base::WeakPtr<ChromotingScriptableObject> scriptable_object,
      MessageLoop* callback_message_loop);

  // Registered the callback class with this object.
  //
  // - This method has subtle thread semantics! -
  //
  // It must be called on the callback thread itself.  The weak pointer also
  // must be constructed on the callback thread.  That means, you cannot just
  // create a WeakPtr on, say the pepper thread, and then pass execution of
  // this function callback with the weak pointer bound as a parameter.  That
  // will fail because the WeakPtr will have been created on the wrong thread.
  virtual void AttachCallback(base::WeakPtr<ResponseCallback> callback);
  virtual void DetachCallback();

  virtual void SendIq(const std::string& request_xml);
  virtual void OnIq(const std::string& response_xml);

 private:
  ~PepperXmppProxy();

  base::WeakPtr<ChromotingScriptableObject> scriptable_object_;

  MessageLoop* callback_message_loop_;

  // Must only be access on callback_message_loop_.
  base::WeakPtr<ResponseCallback> callback_;

  DISALLOW_COPY_AND_ASSIGN(PepperXmppProxy);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_PEPPER_XMPP_PROXY_H_
