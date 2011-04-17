// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_BROKER_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_BROKER_IMPL_H_

#include "base/basictypes.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/trusted/ppb_broker_trusted.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/callbacks.h"
#include "webkit/plugins/ppapi/resource.h"

namespace webkit {
namespace ppapi {

class PluginInstance;

class PPB_Broker_Impl : public Resource {
 public:
  explicit PPB_Broker_Impl(PluginInstance* instance);
  virtual ~PPB_Broker_Impl();

  static const PPB_BrokerTrusted* GetTrustedInterface();

  // PPB_BrokerTrusted implementation.
  int32_t Connect(PluginDelegate* plugin_delegate,
                  PP_CompletionCallback connect_callback);
  int32_t GetHandle(int32_t* handle);

  // Resource override.
  virtual PPB_Broker_Impl* AsPPB_Broker_Impl();

  virtual void BrokerConnected(int32_t handle);

 private:
  // PluginDelegate ppapi broker object.
  // We don't own this pointer but are responsible for calling Release on it.
  scoped_refptr<PluginDelegate::PpapiBroker> broker_;

  // Callback invoked from BrokerConnected.
  scoped_refptr<TrackedCompletionCallback> connect_callback_;

  // Pipe handle for the plugin instance to use to communicate with the broker.
  // Never owned by this object.
  int32_t pipe_handle_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Broker_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_BROKER_IMPL_H_
