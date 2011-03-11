// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_console_proxy.h"

#include "ppapi/c/dev/ppb_console_dev.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace pp {
namespace proxy {

namespace {

void Log(PP_Instance instance, PP_LogLevel_Dev level, PP_Var value) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return;
  dispatcher->Send(new PpapiHostMsg_PPBConsole_Log(
      INTERFACE_ID_PPB_CONSOLE, instance, static_cast<int>(level),
      SerializedVarSendInput(dispatcher, value)));
}

void LogWithSource(PP_Instance instance,
                   PP_LogLevel_Dev level,
                   const PP_Var source,
                   const PP_Var value) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return;
  dispatcher->Send(new PpapiHostMsg_PPBConsole_LogWithSource(
      INTERFACE_ID_PPB_CONSOLE, instance, static_cast<int>(level),
      SerializedVarSendInput(dispatcher, source),
      SerializedVarSendInput(dispatcher, value)));
}

const PPB_Console_Dev console_interface = {
  &Log,
  &LogWithSource
};

InterfaceProxy* CreateConsoleProxy(Dispatcher* dispatcher,
                                   const void* target_interface) {
  return new PPB_Console_Proxy(dispatcher, target_interface);
}

}  // namespace

PPB_Console_Proxy::PPB_Console_Proxy(Dispatcher* dispatcher,
                                     const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_Console_Proxy::~PPB_Console_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_Console_Proxy::GetInfo() {
  static const Info info = {
    &console_interface,
    PPB_CONSOLE_DEV_INTERFACE,
    INTERFACE_ID_PPB_CONSOLE,
    false,
    &CreateConsoleProxy,
  };
  return &info;
}

bool PPB_Console_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Console_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBConsole_Log,
                        OnMsgLog)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBConsole_LogWithSource,
                        OnMsgLogWithSource)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPB_Console_Proxy::OnMsgLog(PP_Instance instance,
                                 int log_level,
                                 SerializedVarReceiveInput value) {
  ppb_console_target()->Log(instance,
                            static_cast<PP_LogLevel_Dev>(log_level),
                            value.Get(dispatcher()));
}

void PPB_Console_Proxy::OnMsgLogWithSource(PP_Instance instance,
                                           int log_level,
                                           SerializedVarReceiveInput source,
                                           SerializedVarReceiveInput value) {
  ppb_console_target()->LogWithSource(
      instance,
      static_cast<PP_LogLevel_Dev>(log_level),
      source.Get(dispatcher()),
      value.Get(dispatcher()));
}

}  // namespace proxy
}  // namespace pp

