// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_ENTER_PROXY_H_
#define PPAPI_PROXY_ENTER_PROXY_H_

#include "base/logging.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/thunk/enter.h"

namespace ppapi {

namespace thunk {
class ResourceCreationAPI;
}

namespace proxy {

// Wrapper around EnterResourceNoLock that takes a host resource. This is used
// when handling messages in the plugin from the host and we need to convert to
// an object in the plugin side corresponding to that.
//
// This never locks since we assume the host Resource is coming from IPC, and
// never logs errors since we assume the host is doing reasonable things.
template<typename ResourceT>
class EnterPluginFromHostResource
    : public thunk::EnterResourceNoLock<ResourceT> {
 public:
  EnterPluginFromHostResource(const HostResource& host_resource)
      : thunk::EnterResourceNoLock<ResourceT>(
            PluginResourceTracker::GetInstance()->PluginResourceForHostResource(
                host_resource),
            false) {
    // Validate that we're in the plugin rather than the host. Otherwise this
    // object will do the wrong thing. In the plugin, the instance should have
    // a corresponding plugin dispatcher (assuming the resource is valid).
    DCHECK(this->failed() ||
           PluginDispatcher::GetForInstance(host_resource.instance()));
  }
};

template<typename ResourceT>
class EnterHostFromHostResource
    : public thunk::EnterResourceNoLock<ResourceT> {
 public:
  EnterHostFromHostResource(const HostResource& host_resource)
      : thunk::EnterResourceNoLock<ResourceT>(
            host_resource.host_resource(), false) {
    // Validate that we're in the host rather than the plugin. Otherwise this
    // object will do the wrong thing. In the host, the instance should have
    // a corresponding host disptacher (assuming the resource is valid).
    DCHECK(this->failed() ||
           HostDispatcher::GetForInstance(host_resource.instance()));
  }
};

// Enters a resource and forces a completion callback to be issued.
//
// This is used when implementing the host (renderer) side of a resource
// function that issues a completion callback. In all cases, we need to issue
// the callback to avoid hanging the plugin.
//
// This class automatically constructs a callback with the given factory
// calling the given method. The method will generally be the one that sends
// the message to trigger the completion callback in the plugin process.
//
// It will automatically issue the callback with PP_ERROR_NOINTERFACE if the
// host resource is invalid (i.e. failed() is set). In all other cases you
// should call SetResult(), which will issue the callback immediately if the
// result value isn't PP_OK_COMPLETIONPENDING. In the "completion pending"
// case, it's assumed the function the proxy is calling will take responsibility
// of executing the callback (returned by callback()).
//
// Example:
//   EnterHostFromHostResourceForceCallback<PPB_Foo_API> enter(
//       resource, callback_factory_, &MyClass::SendResult, resource);
//   if (enter.failed())
//     return;  // SendResult automatically called with PP_ERROR_BADRESOURCE.
//   enter.SetResult(enter.object()->DoFoo(enter.callback()));
//
// Where DoFoo's signature looks like this:
//   int32_t DoFoo(PP_CompletionCallback callback);
// And SendResult's implementation looks like this:
//   void MyClass::SendResult(int32_t result, const HostResource& res) {
//     Send(new FooMsg_FooComplete(..., result, res));
//   }
template<typename ResourceT>
class EnterHostFromHostResourceForceCallback
    : public EnterHostFromHostResource<ResourceT> {
 public:
  // For callbacks that take no parameters except the "int32_t result". Most
  // implementations will use the 1-extra-argument constructor below.
  template<class CallbackFactory, typename Method>
  EnterHostFromHostResourceForceCallback(
      const HostResource& host_resource,
      CallbackFactory& factory,
      Method method)
      : EnterHostFromHostResource<ResourceT>(host_resource),
        needs_running_(true),
        callback_(factory.NewOptionalCallback(method)) {
    if (this->failed())
      RunCallback(PP_ERROR_BADRESOURCE);
  }

  // For callbacks that take an extra parameter as a closure.
  template<class CallbackFactory, typename Method, typename A>
  EnterHostFromHostResourceForceCallback(
      const HostResource& host_resource,
      CallbackFactory& factory,
      Method method,
      const A& a)
      : EnterHostFromHostResource<ResourceT>(host_resource),
        needs_running_(true),
        callback_(factory.NewOptionalCallback(method, a)) {
    if (this->failed())
      RunCallback(PP_ERROR_BADRESOURCE);
  }

  // For callbacks that take two extra parameters as a closure.
  template<class CallbackFactory, typename Method, typename A, typename B>
  EnterHostFromHostResourceForceCallback(
      const HostResource& host_resource,
      CallbackFactory& factory,
      Method method,
      const A& a,
      const B& b)
      : EnterHostFromHostResource<ResourceT>(host_resource),
        needs_running_(true),
        callback_(factory.NewOptionalCallback(method, a, b)) {
    if (this->failed())
      RunCallback(PP_ERROR_BADRESOURCE);
  }

  ~EnterHostFromHostResourceForceCallback() {
    if (needs_running_) {
      NOTREACHED() << "Should always call SetResult except in the "
                      "initialization failed case.";
      RunCallback(PP_ERROR_FAILED);
    }
  }

  void SetResult(int32_t result) {
    DCHECK(needs_running_) << "Don't call SetResult when there already is one.";
    needs_running_ = false;
    if (result != PP_OK_COMPLETIONPENDING)
      callback_.Run(result);
  }

  PP_CompletionCallback callback() {
    return callback_.pp_completion_callback();
  }

 private:
  void RunCallback(int32_t result) {
    DCHECK(needs_running_);
    needs_running_ = false;
    callback_.Run(result);
  }

  bool needs_running_;
  pp::CompletionCallback callback_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_ENTER_PROXY_H_
