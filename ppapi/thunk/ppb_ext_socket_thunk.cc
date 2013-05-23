// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "ppapi/c/extensions/dev/ppb_ext_socket_dev.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/extensions_common_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

int32_t Create(PP_Instance instance,
               PP_Ext_Socket_SocketType_Dev type,
               PP_Ext_Socket_CreateOptions_Dev options,
               PP_Ext_Socket_CreateInfo_Dev* create_info,
               PP_CompletionCallback callback) {
  EnterInstanceAPI<ExtensionsCommon_API> enter(instance, callback);
  if (enter.failed())
    return enter.retval();

  std::vector<PP_Var> input_args;
  std::vector<PP_Var*> output_args;
  input_args.push_back(type);
  input_args.push_back(options);
  output_args.push_back(create_info);
  return enter.SetResult(enter.functions()->CallBrowser(
      "socket.create", input_args, output_args, enter.callback()));
}

void Destroy(PP_Instance instance, PP_Var socket_id) {
  EnterInstanceAPI<ExtensionsCommon_API> enter(instance);
  if (enter.failed())
    return;

  std::vector<PP_Var> args;
  args.push_back(socket_id);
  enter.functions()->PostBrowser("socket.destroy", args);
}

int32_t Connect(PP_Instance instance,
                PP_Var socket_id,
                PP_Var hostname,
                PP_Var port,
                PP_Var* result,
                PP_CompletionCallback callback) {
  EnterInstanceAPI<ExtensionsCommon_API> enter(instance, callback);
  if (enter.failed())
    return enter.retval();

  std::vector<PP_Var> input_args;
  std::vector<PP_Var*> output_args;
  input_args.push_back(socket_id);
  input_args.push_back(hostname);
  input_args.push_back(port);
  output_args.push_back(result);
  return enter.SetResult(enter.functions()->CallBrowser(
      "socket.connect", input_args, output_args, enter.callback()));
}

int32_t Bind(PP_Instance instance,
             PP_Var socket_id,
             PP_Var address,
             PP_Var port,
             PP_Var* result,
             PP_CompletionCallback callback) {
  EnterInstanceAPI<ExtensionsCommon_API> enter(instance, callback);
  if (enter.failed())
    return enter.retval();

  std::vector<PP_Var> input_args;
  std::vector<PP_Var*> output_args;
  input_args.push_back(socket_id);
  input_args.push_back(address);
  input_args.push_back(port);
  output_args.push_back(result);
  return enter.SetResult(enter.functions()->CallBrowser(
      "socket.bind", input_args, output_args, enter.callback()));
}

void Disconnect(PP_Instance instance, PP_Var socket_id) {
  EnterInstanceAPI<ExtensionsCommon_API> enter(instance);
  if (enter.failed())
    return;

  std::vector<PP_Var> args;
  args.push_back(socket_id);
  enter.functions()->PostBrowser("socket.disconnect", args);
}

int32_t Read(PP_Instance instance,
             PP_Var socket_id,
             PP_Var buffer_size,
             PP_Ext_Socket_ReadInfo_Dev* read_info,
             PP_CompletionCallback callback) {
  EnterInstanceAPI<ExtensionsCommon_API> enter(instance, callback);
  if (enter.failed())
    return enter.retval();

  std::vector<PP_Var> input_args;
  std::vector<PP_Var*> output_args;
  input_args.push_back(socket_id);
  input_args.push_back(buffer_size);
  output_args.push_back(read_info);
  return enter.SetResult(enter.functions()->CallBrowser(
      "socket.read", input_args, output_args, enter.callback()));
}

int32_t Write(PP_Instance instance,
              PP_Var socket_id,
              PP_Var data,
              PP_Ext_Socket_WriteInfo_Dev* write_info,
              PP_CompletionCallback callback) {
  EnterInstanceAPI<ExtensionsCommon_API> enter(instance, callback);
  if (enter.failed())
    return enter.retval();

  std::vector<PP_Var> input_args;
  std::vector<PP_Var*> output_args;
  input_args.push_back(socket_id);
  input_args.push_back(data);
  output_args.push_back(write_info);
  return enter.SetResult(enter.functions()->CallBrowser(
      "socket.write", input_args, output_args, enter.callback()));
}

int32_t RecvFrom(PP_Instance instance,
                 PP_Var socket_id,
                 PP_Var buffer_size,
                 PP_Ext_Socket_RecvFromInfo_Dev* recv_from_info,
                 PP_CompletionCallback callback) {
  EnterInstanceAPI<ExtensionsCommon_API> enter(instance, callback);
  if (enter.failed())
    return enter.retval();

  std::vector<PP_Var> input_args;
  std::vector<PP_Var*> output_args;
  input_args.push_back(socket_id);
  input_args.push_back(buffer_size);
  output_args.push_back(recv_from_info);
  return enter.SetResult(enter.functions()->CallBrowser(
      "socket.recvFrom", input_args, output_args, enter.callback()));
}

int32_t SendTo(PP_Instance instance,
               PP_Var socket_id,
               PP_Var data,
               PP_Var address,
               PP_Var port,
               PP_Ext_Socket_WriteInfo_Dev* write_info,
               PP_CompletionCallback callback) {
  EnterInstanceAPI<ExtensionsCommon_API> enter(instance, callback);
  if (enter.failed())
    return enter.retval();

  std::vector<PP_Var> input_args;
  std::vector<PP_Var*> output_args;
  input_args.push_back(socket_id);
  input_args.push_back(data);
  input_args.push_back(address);
  input_args.push_back(port);
  output_args.push_back(write_info);
  return enter.SetResult(enter.functions()->CallBrowser(
      "socket.sendTo", input_args, output_args, enter.callback()));
}

int32_t Listen(PP_Instance instance,
               PP_Var socket_id,
               PP_Var address,
               PP_Var port,
               PP_Var backlog,
               PP_Var* result,
               PP_CompletionCallback callback) {
  EnterInstanceAPI<ExtensionsCommon_API> enter(instance, callback);
  if (enter.failed())
    return enter.retval();

  std::vector<PP_Var> input_args;
  std::vector<PP_Var*> output_args;
  input_args.push_back(socket_id);
  input_args.push_back(address);
  input_args.push_back(port);
  input_args.push_back(backlog);
  output_args.push_back(result);
  return enter.SetResult(enter.functions()->CallBrowser(
      "socket.listen", input_args, output_args, enter.callback()));
}

int32_t Accept(PP_Instance instance,
               PP_Var socket_id,
               PP_Ext_Socket_AcceptInfo_Dev* accept_info,
               PP_CompletionCallback callback) {
  EnterInstanceAPI<ExtensionsCommon_API> enter(instance, callback);
  if (enter.failed())
    return enter.retval();

  std::vector<PP_Var> input_args;
  std::vector<PP_Var*> output_args;
  input_args.push_back(socket_id);
  output_args.push_back(accept_info);
  return enter.SetResult(enter.functions()->CallBrowser(
      "socket.accept", input_args, output_args, enter.callback()));
}

int32_t SetKeepAlive(PP_Instance instance,
                     PP_Var socket_id,
                     PP_Var enable,
                     PP_Var delay,
                     PP_Var* result,
                     PP_CompletionCallback callback) {
  EnterInstanceAPI<ExtensionsCommon_API> enter(instance, callback);
  if (enter.failed())
    return enter.retval();

  std::vector<PP_Var> input_args;
  std::vector<PP_Var*> output_args;
  input_args.push_back(socket_id);
  input_args.push_back(enable);
  input_args.push_back(delay);
  output_args.push_back(result);
  return enter.SetResult(enter.functions()->CallBrowser(
      "socket.setKeepAlive", input_args, output_args, enter.callback()));
}

int32_t SetNoDelay(PP_Instance instance,
                   PP_Var socket_id,
                   PP_Var no_delay,
                   PP_Var* result,
                   PP_CompletionCallback callback) {
  EnterInstanceAPI<ExtensionsCommon_API> enter(instance, callback);
  if (enter.failed())
    return enter.retval();

  std::vector<PP_Var> input_args;
  std::vector<PP_Var*> output_args;
  input_args.push_back(socket_id);
  input_args.push_back(no_delay);
  output_args.push_back(result);
  return enter.SetResult(enter.functions()->CallBrowser(
      "socket.setNoDelay", input_args, output_args, enter.callback()));
}

int32_t GetInfo(PP_Instance instance,
                PP_Var socket_id,
                PP_Ext_Socket_SocketInfo_Dev* result,
                PP_CompletionCallback callback) {
  EnterInstanceAPI<ExtensionsCommon_API> enter(instance, callback);
  if (enter.failed())
    return enter.retval();

  std::vector<PP_Var> input_args;
  std::vector<PP_Var*> output_args;
  input_args.push_back(socket_id);
  output_args.push_back(result);
  return enter.SetResult(enter.functions()->CallBrowser(
      "socket.getInfo", input_args, output_args, enter.callback()));
}

int32_t GetNetworkList(PP_Instance instance,
                       PP_Ext_Socket_NetworkInterface_Dev_Array* result,
                       PP_CompletionCallback callback) {
  EnterInstanceAPI<ExtensionsCommon_API> enter(instance, callback);
  if (enter.failed())
    return enter.retval();

  std::vector<PP_Var> input_args;
  std::vector<PP_Var*> output_args;
  output_args.push_back(result);
  return enter.SetResult(enter.functions()->CallBrowser(
      "socket.getNetworkList", input_args, output_args, enter.callback()));
}

int32_t JoinGroup(PP_Instance instance,
                  PP_Var socket_id,
                  PP_Var address,
                  PP_Var* result,
                  PP_CompletionCallback callback) {
  EnterInstanceAPI<ExtensionsCommon_API> enter(instance, callback);
  if (enter.failed())
    return enter.retval();

  std::vector<PP_Var> input_args;
  std::vector<PP_Var*> output_args;
  input_args.push_back(socket_id);
  input_args.push_back(address);
  output_args.push_back(result);
  return enter.SetResult(enter.functions()->CallBrowser(
      "socket.joinGroup", input_args, output_args, enter.callback()));
}

int32_t LeaveGroup(PP_Instance instance,
                   PP_Var socket_id,
                   PP_Var address,
                   PP_Var* result,
                   PP_CompletionCallback callback) {
  EnterInstanceAPI<ExtensionsCommon_API> enter(instance, callback);
  if (enter.failed())
    return enter.retval();

  std::vector<PP_Var> input_args;
  std::vector<PP_Var*> output_args;
  input_args.push_back(socket_id);
  input_args.push_back(address);
  output_args.push_back(result);
  return enter.SetResult(enter.functions()->CallBrowser(
      "socket.leaveGroup", input_args, output_args, enter.callback()));
}

int32_t SetMulticastTimeToLive(PP_Instance instance,
                               PP_Var socket_id,
                               PP_Var ttl,
                               PP_Var* result,
                               PP_CompletionCallback callback) {
  EnterInstanceAPI<ExtensionsCommon_API> enter(instance, callback);
  if (enter.failed())
    return enter.retval();

  std::vector<PP_Var> input_args;
  std::vector<PP_Var*> output_args;
  input_args.push_back(socket_id);
  input_args.push_back(ttl);
  output_args.push_back(result);
  return enter.SetResult(enter.functions()->CallBrowser(
      "socket.setMulticastTimeToLive", input_args, output_args,
      enter.callback()));
}

int32_t SetMulticastLoopbackMode(PP_Instance instance,
                                 PP_Var socket_id,
                                 PP_Var enabled,
                                 PP_Var* result,
                                 PP_CompletionCallback callback) {
  EnterInstanceAPI<ExtensionsCommon_API> enter(instance, callback);
  if (enter.failed())
    return enter.retval();

  std::vector<PP_Var> input_args;
  std::vector<PP_Var*> output_args;
  input_args.push_back(socket_id);
  input_args.push_back(enabled);
  output_args.push_back(result);
  return enter.SetResult(enter.functions()->CallBrowser(
      "socket.setMulticastLoopbackMode", input_args, output_args,
      enter.callback()));
}

int32_t GetJoinedGroups(PP_Instance instance,
                        PP_Var socket_id,
                        PP_Var* groups,
                        PP_CompletionCallback callback) {
  EnterInstanceAPI<ExtensionsCommon_API> enter(instance, callback);
  if (enter.failed())
    return enter.retval();

  std::vector<PP_Var> input_args;
  std::vector<PP_Var*> output_args;
  input_args.push_back(socket_id);
  output_args.push_back(groups);
  return enter.SetResult(enter.functions()->CallBrowser(
      "socket.getJoinedGroups", input_args, output_args, enter.callback()));
}

const PPB_Ext_Socket_Dev_0_1 g_ppb_ext_socket_dev_0_1_thunk = {
  &Create,
  &Destroy,
  &Connect,
  &Bind,
  &Disconnect,
  &Read,
  &Write,
  &RecvFrom,
  &SendTo,
  &Listen,
  &Accept,
  &SetKeepAlive,
  &SetNoDelay,
  &GetInfo,
  &GetNetworkList
};

const PPB_Ext_Socket_Dev_0_2 g_ppb_ext_socket_dev_0_2_thunk = {
  &Create,
  &Destroy,
  &Connect,
  &Bind,
  &Disconnect,
  &Read,
  &Write,
  &RecvFrom,
  &SendTo,
  &Listen,
  &Accept,
  &SetKeepAlive,
  &SetNoDelay,
  &GetInfo,
  &GetNetworkList,
  &JoinGroup,
  &LeaveGroup,
  &SetMulticastTimeToLive,
  &SetMulticastLoopbackMode,
  &GetJoinedGroups
};
}  // namespace

const PPB_Ext_Socket_Dev_0_1* GetPPB_Ext_Socket_Dev_0_1_Thunk() {
  return &g_ppb_ext_socket_dev_0_1_thunk;
}

const PPB_Ext_Socket_Dev_0_2* GetPPB_Ext_Socket_Dev_0_2_Thunk() {
  return &g_ppb_ext_socket_dev_0_2_thunk;
}

}  // namespace thunk
}  // namespace ppapi
