// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_PEPPER_XMPP_PROXY_H_
#define REMOTING_CLIENT_PLUGIN_PEPPER_XMPP_PROXY_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "remoting/jingle_glue/xmpp_proxy.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

class PepperXmppProxy : public XmppProxy {
 public:
  typedef base::Callback<void(const std::string&)> SendIqCallback;

  // |plugin_task_runner| is the thread on which |send_iq_callback| is
  // called. Normally the callback will call JavaScript, so this has
  // to be the task runner that corresponds to the plugin
  // thread. |callback_task_runner| is used to call the callback
  // registered with AttachCallback().
  PepperXmppProxy(
      const SendIqCallback& send_iq_callback,
      base::SingleThreadTaskRunner* plugin_task_runner,
      base::SingleThreadTaskRunner* callback_task_runner);

  // Registered the callback class with this object.
  //
  // - This method has subtle thread semantics! -
  //
  // It must be called on the callback thread itself.  The weak pointer also
  // must be constructed on the callback thread.  That means, you cannot just
  // create a WeakPtr on, say the pepper thread, and then pass execution of
  // this function callback with the weak pointer bound as a parameter.  That
  // will fail because the WeakPtr will have been created on the wrong thread.
  virtual void AttachCallback(
      base::WeakPtr<ResponseCallback> callback) OVERRIDE;
  virtual void DetachCallback() OVERRIDE;

  virtual void SendIq(const std::string& request_xml) OVERRIDE;
  virtual void OnIq(const std::string& response_xml);

 private:
  virtual ~PepperXmppProxy();

  SendIqCallback send_iq_callback_;

  scoped_refptr<base::SingleThreadTaskRunner> plugin_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> callback_task_runner_;

  // Must only be access on callback_message_loop_.
  base::WeakPtr<ResponseCallback> callback_;

  DISALLOW_COPY_AND_ASSIGN(PepperXmppProxy);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_PEPPER_XMPP_PROXY_H_
