// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PLUGIN_RESOURCE_H_
#define PPAPI_PROXY_PLUGIN_RESOURCE_H_

#include "base/compiler_specific.h"
#include "ipc/ipc_sender.h"
#include "ppapi/proxy/connection.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/resource.h"

namespace IPC {
class Message;
}

namespace ppapi {
namespace proxy {

class PluginDispatcher;

class PPAPI_PROXY_EXPORT PluginResource : public Resource {
 public:
  PluginResource(Connection connection, PP_Instance instance);
  virtual ~PluginResource();

  // Returns true if we've previously sent a create message to the browser
  // or renderer. Generally resources will use these to tell if they should
  // lazily send create messages.
  bool sent_create_to_browser() const { return sent_create_to_browser_; }
  bool sent_create_to_renderer() const { return sent_create_to_renderer_; }

 protected:
  // Sends a create message to the browser or renderer for the current resource.
  void SendCreateToBrowser(const IPC::Message& msg);
  void SendCreateToRenderer(const IPC::Message& msg);

  // Sends the given IPC message as a resource request to the host
  // corresponding to this resource object and does not expect a reply.
  void PostToBrowser(const IPC::Message& msg);
  void PostToRenderer(const IPC::Message& msg);

  // Like PostToBrowser/Renderer but expects a response.
  //
  // Returns the new request's sequence number which can be used to identify
  // the callback. The host will reply and ppapi::Resource::OnReplyReceived
  // will be called.
  //
  // Note that all integers (including 0 and -1) are valid request IDs.
  int32_t CallBrowser(const IPC::Message& msg);
  int32_t CallRenderer(const IPC::Message& msg);

 private:
  Connection connection_;

  int32_t next_sequence_number_;

  bool sent_create_to_browser_;
  bool sent_create_to_renderer_;

  DISALLOW_COPY_AND_ASSIGN(PluginResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PLUGIN_RESOURCE_H_
