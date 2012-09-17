// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <set>

#include "build/build_config.h"
// Need to include this before most other files because it defines
// IPC_MESSAGE_LOG_ENABLED. We need to use it to define
// IPC_MESSAGE_MACROS_LOG_ENABLED so ppapi_messages.h will generate the
// ViewMsgLog et al. functions.

#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_logging.h"
#include "ipc/ipc_message.h"
#include "native_client/src/shared/ppapi_proxy/ppruntime.h"
#include "native_client/src/untrusted/irt/irt_ppapi.h"
#include "ppapi/c/ppp.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/plugin_proxy_delegate.h"
#include "ppapi/shared_impl/ppb_audio_shared.h"

#if defined(IPC_MESSAGE_LOG_ENABLED)
#define IPC_MESSAGE_MACROS_LOG_ENABLED
#endif
#include "ppapi/proxy/ppapi_messages.h"

// This must match up with NACL_CHROME_INITIAL_IPC_DESC,
// defined in sel_main_chrome.h
#define NACL_IPC_FD 6

using ppapi::proxy::PluginDispatcher;
using ppapi::proxy::PluginGlobals;
using ppapi::proxy::PluginProxyDelegate;
using ppapi::proxy::ProxyChannel;
using ppapi::proxy::SerializedHandle;

namespace {

// This class manages communication between the plugin and the browser, and
// manages the PluginDispatcher instances for communication between the plugin
// and the renderer.
class PpapiDispatcher : public ProxyChannel,
                        public PluginDispatcher::PluginDelegate,
                        public PluginProxyDelegate {
 public:
  explicit PpapiDispatcher(scoped_refptr<base::MessageLoopProxy> io_loop);

  // PluginDispatcher::PluginDelegate implementation.
  virtual base::MessageLoopProxy* GetIPCMessageLoop() OVERRIDE;
  virtual base::WaitableEvent* GetShutdownEvent() OVERRIDE;
  virtual IPC::PlatformFileForTransit ShareHandleWithRemote(
      base::PlatformFile handle,
      const IPC::SyncChannel& channel,
      bool should_close_source) OVERRIDE;
  virtual std::set<PP_Instance>* GetGloballySeenInstanceIDSet() OVERRIDE;
  virtual uint32 Register(PluginDispatcher* plugin_dispatcher) OVERRIDE;
  virtual void Unregister(uint32 plugin_dispatcher_id) OVERRIDE;

  // PluginProxyDelegate implementation.
  virtual bool SendToBrowser(IPC::Message* msg) OVERRIDE;
  virtual IPC::Sender* GetBrowserSender() OVERRIDE;
  virtual std::string GetUILanguage() OVERRIDE;
  virtual void PreCacheFont(const void* logfontw) OVERRIDE;
  virtual void SetActiveURL(const std::string& url) OVERRIDE;

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  void OnMsgCreateNaClChannel(int renderer_id,
                              bool incognito,
                              SerializedHandle handle);
  void OnPluginDispatcherMessageReceived(const IPC::Message& msg);

  std::set<PP_Instance> instances_;
  std::map<uint32, PluginDispatcher*> plugin_dispatchers_;
  uint32 next_plugin_dispatcher_id_;
  scoped_refptr<base::MessageLoopProxy> message_loop_;
  base::WaitableEvent shutdown_event_;
};

PpapiDispatcher::PpapiDispatcher(scoped_refptr<base::MessageLoopProxy> io_loop)
    : message_loop_(io_loop),
      shutdown_event_(true, false) {
  IPC::ChannelHandle channel_handle(
      "NaCl IPC", base::FileDescriptor(NACL_IPC_FD, false));
  InitWithChannel(this, channel_handle, false);  // Channel is server.
}

base::MessageLoopProxy* PpapiDispatcher::GetIPCMessageLoop() {
  return message_loop_.get();
}

base::WaitableEvent* PpapiDispatcher::GetShutdownEvent() {
  return &shutdown_event_;
}

IPC::PlatformFileForTransit PpapiDispatcher::ShareHandleWithRemote(
    base::PlatformFile handle,
    const IPC::SyncChannel& channel,
    bool should_close_source) {
  return IPC::InvalidPlatformFileForTransit();
}

std::set<PP_Instance>* PpapiDispatcher::GetGloballySeenInstanceIDSet() {
  return &instances_;
}

uint32 PpapiDispatcher::Register(PluginDispatcher* plugin_dispatcher) {
  if (!plugin_dispatcher ||
      plugin_dispatchers_.size() >= std::numeric_limits<uint32>::max()) {
    return 0;
  }

  uint32 id = 0;
  do {
    // Although it is unlikely, make sure that we won't cause any trouble
    // when the counter overflows.
    id = next_plugin_dispatcher_id_++;
  } while (id == 0 ||
           plugin_dispatchers_.find(id) != plugin_dispatchers_.end());
  plugin_dispatchers_[id] = plugin_dispatcher;
  return id;
}

void PpapiDispatcher::Unregister(uint32 plugin_dispatcher_id) {
  plugin_dispatchers_.erase(plugin_dispatcher_id);
}

bool PpapiDispatcher::SendToBrowser(IPC::Message* msg) {
  Send(msg);
}

IPC::Sender* PpapiDispatcher::GetBrowserSender() {
  return this;
}

std::string PpapiDispatcher::GetUILanguage() {
  NOTIMPLEMENTED();
  return std::string();
}

void PpapiDispatcher::PreCacheFont(const void* logfontw) {
  NOTIMPLEMENTED();
}

void PpapiDispatcher::SetActiveURL(const std::string& url) {
  NOTIMPLEMENTED();
}

bool PpapiDispatcher::OnMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(PpapiDispatcher, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_CreateNaClChannel, OnMsgCreateNaClChannel)

    IPC_MESSAGE_HANDLER_GENERIC(PpapiMsg_PPBTCPServerSocket_ListenACK,
                                OnPluginDispatcherMessageReceived(msg))
    IPC_MESSAGE_HANDLER_GENERIC(PpapiMsg_PPBTCPServerSocket_AcceptACK,
                                OnPluginDispatcherMessageReceived(msg))
    IPC_MESSAGE_HANDLER_GENERIC(PpapiMsg_PPBTCPSocket_ConnectACK,
                                OnPluginDispatcherMessageReceived(msg))
    IPC_MESSAGE_HANDLER_GENERIC(PpapiMsg_PPBTCPSocket_SSLHandshakeACK,
                                OnPluginDispatcherMessageReceived(msg))
    IPC_MESSAGE_HANDLER_GENERIC(PpapiMsg_PPBTCPSocket_ReadACK,
                                OnPluginDispatcherMessageReceived(msg))
    IPC_MESSAGE_HANDLER_GENERIC(PpapiMsg_PPBTCPSocket_WriteACK,
                                OnPluginDispatcherMessageReceived(msg))
    IPC_MESSAGE_HANDLER_GENERIC(PpapiMsg_PPBUDPSocket_RecvFromACK,
                                OnPluginDispatcherMessageReceived(msg))
    IPC_MESSAGE_HANDLER_GENERIC(PpapiMsg_PPBUDPSocket_SendToACK,
                                OnPluginDispatcherMessageReceived(msg))
    IPC_MESSAGE_HANDLER_GENERIC(PpapiMsg_PPBUDPSocket_BindACK,
                                OnPluginDispatcherMessageReceived(msg))
  IPC_END_MESSAGE_MAP()
  return true;
}

void PpapiDispatcher::OnMsgCreateNaClChannel(int renderer_id,
                                             bool incognito,
                                             SerializedHandle handle) {
  PluginDispatcher* dispatcher =
      new PluginDispatcher(::PPP_GetInterface, incognito);
  // The channel handle's true name is not revealed here.
  IPC::ChannelHandle channel_handle("nacl", handle.descriptor());
  if (!dispatcher->InitPluginWithChannel(this, channel_handle, false)) {
    delete dispatcher;
    return;
  }
  // From here, the dispatcher will manage its own lifetime according to the
  // lifetime of the attached channel.
}

void PpapiDispatcher::OnPluginDispatcherMessageReceived(
    const IPC::Message& msg) {
  // The first parameter should be a plugin dispatcher ID.
  PickleIterator iter(msg);
  uint32 id = 0;
  if (!msg.ReadUInt32(&iter, &id)) {
    NOTREACHED();
    return;
  }
  std::map<uint32, ppapi::proxy::PluginDispatcher*>::iterator dispatcher =
      plugin_dispatchers_.find(id);
  if (dispatcher != plugin_dispatchers_.end())
    dispatcher->second->OnMessageReceived(msg);
}

}  // namespace

void PpapiPluginRegisterThreadCreator(
    const struct PP_ThreadFunctions* thread_functions) {
  // Initialize all classes that need to create threads that call back into
  // user code.
  ppapi::PPB_Audio_Shared::SetThreadFunctions(thread_functions);
}

int IrtInit() {
  return 0;
}

int PpapiPluginMain() {
  base::AtExitManager exit_manager;
  MessageLoop loop;
  IPC::Logging::set_log_function_map(&g_log_function_mapping);
  ppapi::proxy::PluginGlobals plugin_globals;
  base::Thread io_thread("Chrome_NaClIOThread");
  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  io_thread.StartWithOptions(options);

  int32_t error = ::PPP_InitializeModule(
      0 /* module */,
      &ppapi::proxy::PluginDispatcher::GetBrowserInterface);
  // TODO(dmichael): Handle other error conditions, like failure to connect?
  if (error)
    return error;

  PpapiDispatcher ppapi_dispatcher(io_thread.message_loop_proxy());

  loop.Run();
  return 0;
}

