// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PLUGIN_RESOURCE_CALLBACK_H_
#define PPAPI_PROXY_PLUGIN_RESOURCE_CALLBACK_H_

#include "base/memory/ref_counted.h"
#include "ipc/ipc_message.h"
#include "ppapi/proxy/dispatch_reply_message.h"
#include "ppapi/proxy/resource_message_params.h"

namespace ppapi {
namespace proxy {

// |PluginResourceCallback| wraps a |base::Callback| on the plugin side which
// will be triggered in response to a particular message type being received.
// |MsgClass| is the reply message type that the callback will be called with
// and |CallbackType| is the type of the |base::Callback| that will be called.
class PluginResourceCallbackBase
    : public base::RefCounted<PluginResourceCallbackBase> {
 public:
  virtual void Run(const ResourceMessageReplyParams& params,
                   const IPC::Message& msg) = 0;
 protected:
  friend class base::RefCounted<PluginResourceCallbackBase>;
  virtual ~PluginResourceCallbackBase() {}
};

template<typename MsgClass, typename CallbackType>
class PluginResourceCallback : public PluginResourceCallbackBase {
 public:
  explicit PluginResourceCallback(const CallbackType& callback)
      : callback_(callback) {}

  void Run(
      const ResourceMessageReplyParams& reply_params,
      const IPC::Message& msg) override {
    DispatchResourceReplyOrDefaultParams<MsgClass>(callback_, reply_params,
                                                   msg);
  }

 private:
  ~PluginResourceCallback() override {}

  CallbackType callback_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PLUGIN_RESOURCE_CALLBACK_H_
