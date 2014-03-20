// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/plugin_main_irt.h"

#include <unistd.h>
#include <map>
#include <set>

#include "build/build_config.h"
// Need to include this before most other files because it defines
// IPC_MESSAGE_LOG_ENABLED. We need to use it to define
// IPC_MESSAGE_MACROS_LOG_ENABLED so ppapi_messages.h will generate the
// ViewMsgLog et al. functions.

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "components/tracing/child_trace_message_filter.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_logging.h"
#include "ipc/ipc_message.h"
#include "ppapi/c/ppp.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/plugin_message_filter.h"
#include "ppapi/proxy/plugin_proxy_delegate.h"
#include "ppapi/proxy/resource_reply_thread_registrar.h"
#include "ppapi/shared_impl/ppapi_switches.h"
#include "ppapi/shared_impl/ppb_audio_shared.h"

#if defined(__native_client__)
#include "base/at_exit.h"
#include "native_client/src/public/chrome_main.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#endif

#if defined(IPC_MESSAGE_LOG_ENABLED)
#include "base/containers/hash_tables.h"

LogFunctionMap g_log_function_mapping;

#define IPC_MESSAGE_MACROS_LOG_ENABLED
#define IPC_LOG_TABLE_ADD_ENTRY(msg_id, logger) \
  g_log_function_mapping[msg_id] = logger

#endif
#include "ppapi/proxy/ppapi_messages.h"

using ppapi::proxy::PluginDispatcher;
using ppapi::proxy::PluginGlobals;
using ppapi::proxy::PluginProxyDelegate;
using ppapi::proxy::ProxyChannel;
using ppapi::proxy::SerializedHandle;

namespace {

#if defined(__native_client__)
// In SFI mode, the FDs of IPC channels are NACL_CHROME_DESC_BASE and its
// successor, which is set in nacl_listener.cc.
int g_nacl_ipc_browser_fd = NACL_CHROME_DESC_BASE;
int g_nacl_ipc_renderer_fd = NACL_CHROME_DESC_BASE + 1;
#else
// In non-SFI mode, the FDs of IPC channels are different from the hard coded
// ones. These values will be set by SetIPCFileDescriptors() below.
// At first, both are initialized to invalid FD number (-1).
int g_nacl_ipc_browser_fd = -1;
int g_nacl_ipc_renderer_fd = -1;
#endif

// This class manages communication between the plugin and the browser, and
// manages the PluginDispatcher instances for communication between the plugin
// and the renderer.
class PpapiDispatcher : public PluginDispatcher::PluginDelegate,
                        public PluginProxyDelegate,
                        public IPC::Listener,
                        public IPC::Sender {
 public:
  explicit PpapiDispatcher(scoped_refptr<base::MessageLoopProxy> io_loop);

  // PluginDispatcher::PluginDelegate implementation.
  virtual base::MessageLoopProxy* GetIPCMessageLoop() OVERRIDE;
  virtual base::WaitableEvent* GetShutdownEvent() OVERRIDE;
  virtual IPC::PlatformFileForTransit ShareHandleWithRemote(
      base::PlatformFile handle,
      base::ProcessId peer_pid,
      bool should_close_source) OVERRIDE;
  virtual std::set<PP_Instance>* GetGloballySeenInstanceIDSet() OVERRIDE;
  virtual uint32 Register(PluginDispatcher* plugin_dispatcher) OVERRIDE;
  virtual void Unregister(uint32 plugin_dispatcher_id) OVERRIDE;

  // PluginProxyDelegate implementation.
  virtual IPC::Sender* GetBrowserSender() OVERRIDE;
  virtual std::string GetUILanguage() OVERRIDE;
  virtual void PreCacheFont(const void* logfontw) OVERRIDE;
  virtual void SetActiveURL(const std::string& url) OVERRIDE;
  virtual PP_Resource CreateBrowserFont(
      ppapi::proxy::Connection connection,
      PP_Instance instance,
      const PP_BrowserFont_Trusted_Description& desc,
      const ppapi::Preferences& prefs) OVERRIDE;

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  // IPC::Sender implementation
  virtual bool Send(IPC::Message* message) OVERRIDE;

 private:
  void OnMsgInitializeNaClDispatcher(const ppapi::PpapiNaClPluginArgs& args);
  void OnPluginDispatcherMessageReceived(const IPC::Message& msg);

  void SetPpapiKeepAliveThrottleFromCommandLine();

  std::set<PP_Instance> instances_;
  std::map<uint32, PluginDispatcher*> plugin_dispatchers_;
  uint32 next_plugin_dispatcher_id_;
  scoped_refptr<base::MessageLoopProxy> message_loop_;
  base::WaitableEvent shutdown_event_;
  scoped_ptr<IPC::SyncChannel> channel_;
};

PpapiDispatcher::PpapiDispatcher(scoped_refptr<base::MessageLoopProxy> io_loop)
    : next_plugin_dispatcher_id_(0),
      message_loop_(io_loop),
      shutdown_event_(true, false) {
  DCHECK_NE(g_nacl_ipc_browser_fd, -1)
      << "g_nacl_ipc_browser_fd must be initialized before the plugin starts";
  IPC::ChannelHandle channel_handle(
      "NaCl IPC", base::FileDescriptor(g_nacl_ipc_browser_fd, false));

  // Delay initializing the SyncChannel until after we add filters. This
  // ensures that the filters won't miss any messages received by
  // the channel.
  channel_.reset(new IPC::SyncChannel(
      this, GetIPCMessageLoop(), GetShutdownEvent()));
  channel_->AddFilter(new ppapi::proxy::PluginMessageFilter(
      NULL, PluginGlobals::Get()->resource_reply_thread_registrar()));
  channel_->AddFilter(
      new tracing::ChildTraceMessageFilter(message_loop_.get()));
  channel_->Init(channel_handle, IPC::Channel::MODE_SERVER, true);
}

base::MessageLoopProxy* PpapiDispatcher::GetIPCMessageLoop() {
  return message_loop_.get();
}

base::WaitableEvent* PpapiDispatcher::GetShutdownEvent() {
  return &shutdown_event_;
}

IPC::PlatformFileForTransit PpapiDispatcher::ShareHandleWithRemote(
    base::PlatformFile handle,
    base::ProcessId peer_pid,
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

PP_Resource PpapiDispatcher::CreateBrowserFont(
    ppapi::proxy::Connection connection,
    PP_Instance instance,
    const PP_BrowserFont_Trusted_Description& desc,
    const ppapi::Preferences& prefs) {
  NOTIMPLEMENTED();
  return 0;
}

bool PpapiDispatcher::OnMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(PpapiDispatcher, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_InitializeNaClDispatcher,
                        OnMsgInitializeNaClDispatcher)
    // All other messages are simply forwarded to a PluginDispatcher.
    IPC_MESSAGE_UNHANDLED(OnPluginDispatcherMessageReceived(msg))
  IPC_END_MESSAGE_MAP()
  return true;
}

void PpapiDispatcher::OnChannelError() {
  exit(1);
}

bool PpapiDispatcher::Send(IPC::Message* msg) {
  return channel_->Send(msg);
}

void PpapiDispatcher::OnMsgInitializeNaClDispatcher(
    const ppapi::PpapiNaClPluginArgs& args) {
  static bool command_line_and_logging_initialized = false;
  if (command_line_and_logging_initialized) {
    LOG(FATAL) << "InitializeNaClDispatcher must be called once per plugin.";
    return;
  }

  command_line_and_logging_initialized = true;
  CommandLine::Init(0, NULL);
  for (size_t i = 0; i < args.switch_names.size(); ++i) {
    DCHECK(i < args.switch_values.size());
    CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        args.switch_names[i], args.switch_values[i]);
  }
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  logging::InitLogging(settings);
  SetPpapiKeepAliveThrottleFromCommandLine();

  // Tell the process-global GetInterface which interfaces it can return to the
  // plugin.
  ppapi::proxy::InterfaceList::SetProcessGlobalPermissions(
      args.permissions);

  int32_t error = ::PPP_InitializeModule(
      0 /* module */,
      &ppapi::proxy::PluginDispatcher::GetBrowserInterface);
  if (error)
    ::exit(error);

  PluginDispatcher* dispatcher =
      new PluginDispatcher(::PPP_GetInterface, args.permissions,
                           args.off_the_record);
  // The channel handle's true name is not revealed here.
  DCHECK_NE(g_nacl_ipc_renderer_fd, -1)
      << "g_nacl_ipc_renderer_fd must be initialized before the plugin starts";
  IPC::ChannelHandle channel_handle(
      "nacl", base::FileDescriptor(g_nacl_ipc_renderer_fd, false));
  if (!dispatcher->InitPluginWithChannel(this, base::kNullProcessId,
                                         channel_handle, false)) {
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

void PpapiDispatcher::SetPpapiKeepAliveThrottleFromCommandLine() {
  unsigned keepalive_throttle_interval_milliseconds = 0;
  if (base::StringToUint(
          CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              switches::kPpapiKeepAliveThrottle),
          &keepalive_throttle_interval_milliseconds)) {
    ppapi::proxy::PluginGlobals::Get()->
        set_keepalive_throttle_interval_milliseconds(
            keepalive_throttle_interval_milliseconds);
  }
}

}  // namespace

void SetIPCFileDescriptors(int ipc_browser_fd, int ipc_renderer_fd) {
  g_nacl_ipc_browser_fd = ipc_browser_fd;
  g_nacl_ipc_renderer_fd = ipc_renderer_fd;
}

void PpapiPluginRegisterThreadCreator(
    const struct PP_ThreadFunctions* thread_functions) {
#if defined(__native_client__)
  // TODO(hidehiko): The thread creation for the PPB_Audio is not yet
  // implemented on non-SFI mode. Support this. Now, this function invocation
  // is just ignored.

  // Initialize all classes that need to create threads that call back into
  // user code.
  ppapi::PPB_Audio_Shared::SetThreadFunctions(thread_functions);
#endif
}

int PpapiPluginMain() {
  // For non-SFI mode, the manager is already instantiated in nacl_helper,
  // so we don't need to instantiate it here.
#if defined(__native_client__)
  // Though it isn't referenced here, we must instantiate an AtExitManager.
  base::AtExitManager exit_manager;
#endif
  base::MessageLoop loop;
#if defined(IPC_MESSAGE_LOG_ENABLED)
  IPC::Logging::set_log_function_map(&g_log_function_mapping);
#endif
  ppapi::proxy::PluginGlobals plugin_globals;
  base::Thread io_thread("Chrome_NaClIOThread");
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  io_thread.StartWithOptions(options);

#if defined(__native_client__)
  // Currently on non-SFI mode, we don't use SRPC server on plugin.
  // TODO(hidehiko): Make sure this SRPC is actually used on SFI-mode.

  // Start up the SRPC server on another thread. Otherwise, when it blocks
  // on an RPC, the PPAPI proxy will hang. Do this before we initialize the
  // module and start the PPAPI proxy so that the NaCl plugin can continue
  // loading the app.
  static struct NaClSrpcHandlerDesc srpc_methods[] = { { NULL, NULL } };
  if (!NaClSrpcAcceptClientOnThread(srpc_methods)) {
    return 1;
  }
#endif

  PpapiDispatcher ppapi_dispatcher(io_thread.message_loop_proxy());
  plugin_globals.set_plugin_proxy_delegate(&ppapi_dispatcher);

  loop.Run();

  return 0;
}
