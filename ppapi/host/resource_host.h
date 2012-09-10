// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_HOST_RESOURCE_HOST_H_
#define PPAPI_HOST_RESOURCE_HOST_H_

#include "base/basictypes.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/host/ppapi_host_export.h"
#include "ppapi/shared_impl/host_resource.h"

namespace IPC {
class Message;
}

namespace ppapi {
namespace host {

struct HostMessageContext;
class PpapiHost;

// Some (but not all) resources have a corresponding object in the host side
// that is kept alive as long as the resource in the plugin is alive. This is
// the base class for such objects.
class PPAPI_HOST_EXPORT ResourceHost {
 public:
  ResourceHost(PpapiHost* host, PP_Instance instance, PP_Resource resource);
  virtual ~ResourceHost();

  PpapiHost* host() { return host_; }
  PP_Instance pp_instance() const { return pp_instance_; }
  PP_Resource pp_resource() const { return pp_resource_; }

  // Handles messages associated with a given resource object. If the flags
  // indicate that a response is required, the return value of this function
  // will be sent as a resource message "response" along with the message
  // specified in the reply of the context.
  //
  // You can do a response asynchronously by returning PP_OK_COMPLETIONPENDING.
  // This will cause the reply to be skipped, and the class implementing this
  // function will take responsibility for issuing the callback. The callback
  // can be issued inside OnResourceMessageReceived before it returns, or at
  // a future time.
  //
  // If you don't have a particular reply message, you can just ignore
  // the reply in the message context. However, if you have a reply more than
  // just the int32_t result code, set the reply to be the message of your
  // choosing.
  //
  // The default implementation just returns PP_ERROR_NOTSUPPORTED.
  virtual int32_t OnResourceMessageReceived(const IPC::Message& msg,
                                            HostMessageContext* context);

 private:
  // The host that owns this object.
  PpapiHost* host_;

  PP_Instance pp_instance_;
  PP_Resource pp_resource_;

  DISALLOW_COPY_AND_ASSIGN(ResourceHost);
};

}  // namespace host
}  // namespace ppapi

#endif  // PPAPI_HOST_RESOURCE_HOST_H_
