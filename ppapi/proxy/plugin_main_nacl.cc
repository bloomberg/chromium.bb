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

#if defined(IPC_MESSAGE_LOG_ENABLED)
#define IPC_MESSAGE_MACROS_LOG_ENABLED
#include "ppapi/proxy/ppapi_messages.h"
#endif

// This must match up with NACL_CHROME_INITIAL_IPC_DESC,
// defined in sel_main_chrome.h
#define NACL_IPC_FD 6

using ppapi::proxy::PluginDispatcher;
using ppapi::proxy::PluginGlobals;

namespace {

struct PP_ThreadFunctions thread_funcs;

// Copied from src/content/ppapi_plugin/ppapi_thread. This is a minimal
// implementation to get us started.
class PluginDispatcherDelegate : public PluginDispatcher::PluginDelegate {
 public:
  explicit PluginDispatcherDelegate(
      scoped_refptr<base::MessageLoopProxy> io_loop)
      : message_loop_(io_loop),
        shutdown_event_(true, false) {
  }

  virtual base::MessageLoopProxy* GetIPCMessageLoop() OVERRIDE {
     return message_loop_.get();
  }

  virtual base::WaitableEvent* GetShutdownEvent() OVERRIDE {
    return &shutdown_event_;
  }

  virtual IPC::PlatformFileForTransit ShareHandleWithRemote(
      base::PlatformFile handle,
      const IPC::SyncChannel& channel,
      bool should_close_source) OVERRIDE {
    return IPC::InvalidPlatformFileForTransit();
  }

  virtual std::set<PP_Instance>* GetGloballySeenInstanceIDSet() OVERRIDE {
    return &instances_;
  }

  virtual uint32 Register(PluginDispatcher* plugin_dispatcher) OVERRIDE {
    if (!plugin_dispatcher ||
        plugin_dispatchers_.size() >= std::numeric_limits<uint32>::max()) {
      return 0;
    }

    uint32 id = 0;
    do {
      // Although it is unlikely, make sure that we won't cause any trouble when
      // the counter overflows.
      id = next_plugin_dispatcher_id_++;
    } while (id == 0 ||
             plugin_dispatchers_.find(id) != plugin_dispatchers_.end());
    plugin_dispatchers_[id] = plugin_dispatcher;
    return id;
  }

  virtual void Unregister(uint32 plugin_dispatcher_id) OVERRIDE {
    plugin_dispatchers_.erase(plugin_dispatcher_id);
  }

 private:
  std::set<PP_Instance> instances_;
  std::map<uint32, PluginDispatcher*> plugin_dispatchers_;
  uint32 next_plugin_dispatcher_id_;
  scoped_refptr<base::MessageLoopProxy> message_loop_;
  base::WaitableEvent shutdown_event_;
};

}  // namespace

void PpapiPluginRegisterThreadCreator(
    const struct PP_ThreadFunctions* new_funcs) {
  thread_funcs = *new_funcs;
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

  PluginDispatcherDelegate delegate(io_thread.message_loop_proxy());

  // TODO(dmichael) Figure out how to determine if we're in incognito
  PluginDispatcher dispatcher(::PPP_GetInterface, false /* incognito */);
  IPC::ChannelHandle channel_handle("NaCl IPC",
                                    base::FileDescriptor(NACL_IPC_FD, false));
  dispatcher.InitPluginWithChannel(&delegate, channel_handle, false);

  loop.Run();
  return 0;
}

