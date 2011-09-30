// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_BROKER_DISPATCHER_H_
#define PPAPI_PROXY_BROKER_DISPATCHER_H_

#include "ppapi/c/trusted/ppp_broker.h"
#include "ppapi/proxy/proxy_channel.h"

namespace ppapi {
namespace proxy {

class PPAPI_PROXY_EXPORT BrokerDispatcher : public ProxyChannel {
 public:
  virtual ~BrokerDispatcher();

  // You must call this function before anything else. Returns true on success.
  // The delegate pointer must outlive this class, ownership is not
  // transferred.
  virtual bool InitBrokerWithChannel(ProxyChannel::Delegate* delegate,
                                     const IPC::ChannelHandle& channel_handle,
                                     bool is_client);

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 protected:
  // You must call InitBrokerWithChannel after the constructor.
  BrokerDispatcher(base::ProcessHandle remote_process_handle,
                   PP_ConnectInstance_Func connect_instance);

  void OnMsgConnectToPlugin(PP_Instance instance,
                            IPC::PlatformFileForTransit handle,
                            int32_t* result);

  PP_ConnectInstance_Func connect_instance_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrokerDispatcher);
};

// The dispatcher for the browser side of the broker channel.
class PPAPI_PROXY_EXPORT BrokerHostDispatcher : public BrokerDispatcher {
 public:
  BrokerHostDispatcher(base::ProcessHandle remote_process_handle);

  // IPC::Channel::Listener implementation.
  virtual void OnChannelError();
};

// The dispatcher for the broker side of the broker channel.
class PPAPI_PROXY_EXPORT BrokerSideDispatcher : public BrokerDispatcher {
 public:
  BrokerSideDispatcher(base::ProcessHandle remote_process_handle,
                       PP_ConnectInstance_Func connect_instance);

  // IPC::Channel::Listener implementation.
  virtual void OnChannelError();
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_BROKER_DISPATCHER_H_
