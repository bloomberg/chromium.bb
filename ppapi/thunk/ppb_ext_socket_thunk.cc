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

// TODO(yzshen): The socket API should directly communicate with the browser
// process, instead of going by way of the renderer process.

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
  return enter.SetResult(enter.functions()->Call(
      "socket.create", input_args, output_args, enter.callback()));
}

void Destroy(PP_Instance instance, PP_Var socket_id) {
  EnterInstanceAPI<ExtensionsCommon_API> enter(instance);
  if (enter.failed())
    return;

  std::vector<PP_Var> args;
  args.push_back(socket_id);
  enter.functions()->Post("socket.destroy", args);
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
  return enter.SetResult(enter.functions()->Call(
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
  return enter.SetResult(enter.functions()->Call(
      "socket.bind", input_args, output_args, enter.callback()));
}

void Disconnect(PP_Instance instance, PP_Var socket_id) {
  EnterInstanceAPI<ExtensionsCommon_API> enter(instance);
  if (enter.failed())
    return;

  std::vector<PP_Var> args;
  args.push_back(socket_id);
  enter.functions()->Post("socket.disconnect", args);
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
  return enter.SetResult(enter.functions()->Call(
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
  return enter.SetResult(enter.functions()->Call(
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
  return enter.SetResult(enter.functions()->Call(
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
  return enter.SetResult(enter.functions()->Call(
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
  return enter.SetResult(enter.functions()->Call(
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
  return enter.SetResult(enter.functions()->Call(
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
  return enter.SetResult(enter.functions()->Call(
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
  return enter.SetResult(enter.functions()->Call(
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
  return enter.SetResult(enter.functions()->Call(
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
  return enter.SetResult(enter.functions()->Call(
      "socket.getNetworkList", input_args, output_args, enter.callback()));
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

}  // namespace

const PPB_Ext_Socket_Dev_0_1* GetPPB_Ext_Socket_Dev_0_1_Thunk() {
  return &g_ppb_ext_socket_dev_0_1_thunk;
}

}  // namespace thunk
}  // namespace ppapi
