// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_BROKER_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_BROKER_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/trusted/ppb_broker_trusted.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_broker_api.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/callbacks.h"
#include "webkit/plugins/webkit_plugins_export.h"

namespace webkit {
namespace ppapi {

class WEBKIT_PLUGINS_EXPORT PPB_Broker_Impl
    : public ::ppapi::Resource,
      NON_EXPORTED_BASE(public ::ppapi::thunk::PPB_Broker_API),
      public base::SupportsWeakPtr<PPB_Broker_Impl> {
 public:
  explicit PPB_Broker_Impl(PP_Instance instance);
  virtual ~PPB_Broker_Impl();

  // Resource override.
  virtual ::ppapi::thunk::PPB_Broker_API* AsPPB_Broker_API() OVERRIDE;

  // PPB_BrokerTrusted implementation.
  virtual int32_t Connect(PP_CompletionCallback connect_callback) OVERRIDE;
  virtual int32_t GetHandle(int32_t* handle) OVERRIDE;

  void BrokerConnected(int32_t handle, int32_t result);

 private:
  // PluginDelegate ppapi broker object.
  // We don't own this pointer but are responsible for calling Disconnect on it.
  PluginDelegate::PpapiBroker* broker_;

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
