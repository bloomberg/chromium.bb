// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PLUGIN_MESSAGE_FILTER_H_
#define PPAPI_PROXY_PLUGIN_MESSAGE_FILTER_H_

#include <set>

#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_message.h"
#include "ppapi/c/pp_instance.h"

namespace pp {
namespace proxy {

// Listens for messages on the I/O thread of the plugin and handles some of
// them to avoid needing to block on the plugin.
//
// There is one instance of this class for each renderer channel (same as for
// the PluginDispatchers).
class PluginMessageFilter : public IPC::ChannelProxy::MessageFilter,
                            public IPC::Message::Sender {
 public:
  // The input is a pointer to a set that will be used to uniquify PP_Instances
  // across all renderer channels. The same pointer should be passed to each
  // MessageFilter to ensure uniqueness, and the value should outlive this
  // class.
  PluginMessageFilter(std::set<PP_Instance>* seen_instance_ids);
  virtual ~PluginMessageFilter();

  // MessageFilter implementation.
  virtual void OnFilterAdded(IPC::Channel* channel);
  virtual void OnFilterRemoved();
  virtual bool OnMessageReceived(const IPC::Message& message);

  // Message::Sender implementation.
  virtual bool Send(IPC::Message* msg);

 private:
  void OnMsgReserveInstanceId(PP_Instance instance, bool* usable);

  // All instance IDs every queried by any renderer on this plugin. This is
  // used to make sure that new instance IDs are unique. This is a non-owning
  // pointer, it will be managed by the later that creates this class.
  std::set<PP_Instance>* seen_instance_ids_;

  // The IPC channel to the renderer. May be NULL if we're not currently
  // attached as a filter.
  IPC::Channel* channel_;
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_PLUGIN_MESSAGE_FILTER_H_
