// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The XmppProxy is a shim interface that allows a class from layers above
// the protocol to insert custom logic for dispatching the XMPP requests
// necessary for creating a jingle connection.
//
// The primary motivator for this is to allow libjingle to be sandboxed in the
// client by proxying the XMPP requests up through javascript into a
// javascript-based XMPP connection back into the GoogleTalk network.  It's
// essentially a clean hack.

#ifndef REMOTING_JINGLE_GLUE_XMPP_PROXY_H_
#define REMOTING_JINGLE_GLUE_XMPP_PROXY_H_

#include <string>

#include "base/ref_counted.h"
#include "base/weak_ptr.h"

class MessageLoop;

namespace remoting {

class XmppProxy : public base::RefCountedThreadSafe<XmppProxy> {
 public:
  XmppProxy() {}

  class ResponseCallback : public base::SupportsWeakPtr<ResponseCallback> {
   public:
    ResponseCallback() {}
    virtual ~ResponseCallback() {}
    virtual void OnIq(const std::string& response_xml) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(ResponseCallback);
  };

  // These two must be called on the callback's message_loop. Callback will
  // always been run on the callback_loop.
  virtual void AttachCallback(base::WeakPtr<ResponseCallback> callback) = 0;
  virtual void DetachCallback() = 0;

  virtual void SendIq(const std::string& iq_request_xml) = 0;

 protected:
  friend class base::RefCountedThreadSafe<XmppProxy>;
  virtual ~XmppProxy() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(XmppProxy);
};

}  // namespace remoting

#endif // REMOTING_JINGLE_GLUE_XMPP_PROXY_H_
