// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_broker_impl.h"

#include "base/logging.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/plugin_module.h"

namespace webkit {
namespace ppapi {

namespace {

// PPB_BrokerTrusted ----------------------------------------------------

PP_Resource CreateTrusted(PP_Instance instance_id) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return 0;
  scoped_refptr<PPB_Broker_Impl> broker(new PPB_Broker_Impl(instance));
  return broker->GetReference();
}

PP_Bool IsBrokerTrusted(PP_Resource resource) {
  scoped_refptr<PPB_Broker_Impl> broker =
      Resource::GetAs<PPB_Broker_Impl>(resource);
  return BoolToPPBool(!!broker);
}

int32_t Connect(PP_Resource broker_id,
                PP_CompletionCallback connect_callback) {
  scoped_refptr<PPB_Broker_Impl> broker =
      Resource::GetAs<PPB_Broker_Impl>(broker_id);
  if (!broker)
    return PP_ERROR_BADRESOURCE;
  if (!connect_callback.func) {
    // Synchronous calls are not supported.
    return PP_ERROR_BADARGUMENT;
  }
  return broker->Connect(broker->instance()->delegate(), connect_callback);
}

int32_t GetHandle(PP_Resource broker_id, int32_t* handle) {
  scoped_refptr<PPB_Broker_Impl> broker =
      Resource::GetAs<PPB_Broker_Impl>(broker_id);
  if (!broker)
    return PP_ERROR_BADRESOURCE;
  if (!handle)
    return PP_ERROR_BADARGUMENT;
  return broker->GetHandle(handle);
}

const PPB_BrokerTrusted ppb_brokertrusted = {
  &CreateTrusted,
  &IsBrokerTrusted,
  &Connect,
  &GetHandle,
};

}  // namespace

// PPB_Broker_Impl ------------------------------------------------------

PPB_Broker_Impl::PPB_Broker_Impl(PluginInstance* instance)
    : Resource(instance),
      broker_(NULL),
      connect_callback_(),
      pipe_handle_(0) {
}

PPB_Broker_Impl::~PPB_Broker_Impl() {
  if (broker_) {
    broker_->Release(instance());
    broker_ = NULL;
  }

  // TODO(ddorwin): Should the plugin or Chrome free the handle?
  pipe_handle_ = 0;
}

const PPB_BrokerTrusted* PPB_Broker_Impl::GetTrustedInterface() {
  return &ppb_brokertrusted;
}

int32_t PPB_Broker_Impl::Connect(
    PluginDelegate* plugin_delegate,
    PP_CompletionCallback connect_callback) {
  // TODO(ddorwin): Return PP_ERROR_FAILED if plugin is in-process.

  if (broker_) {
    // May only be called once.
    return PP_ERROR_FAILED;
  }

  broker_ = plugin_delegate->ConnectToPpapiBroker(instance(), this);
  if (!broker_)
    return PP_ERROR_FAILED;

  PP_Resource resource_id = GetReferenceNoAddRef();
  CHECK(resource_id);
  connect_callback_ = new TrackedCompletionCallback(
      instance()->module()->GetCallbackTracker(), resource_id,
      connect_callback);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_Broker_Impl::GetHandle(int32_t* handle) {
  *handle = pipe_handle_;

  if (!*handle)
    return PP_ERROR_FAILED;

  return PP_OK;
}

PPB_Broker_Impl* PPB_Broker_Impl::AsPPB_Broker_Impl() {
  return this;
}

void PPB_Broker_Impl::BrokerConnected(int32_t handle) {
  DCHECK(handle);
  pipe_handle_ = handle;

  // Synchronous calls are not supported.
  DCHECK(connect_callback_.get() && !connect_callback_->completed());

  scoped_refptr<TrackedCompletionCallback> callback;
  callback.swap(connect_callback_);
  callback->Run(0);  // Will complete abortively if necessary.
}

}  // namespace ppapi
}  // namespace webkit
