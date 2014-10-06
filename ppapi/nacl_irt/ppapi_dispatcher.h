// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_NACL_IRT_PPAPI_DISPATCHER_H_
#define PPAPI_NACL_IRT_PPAPI_DISPATCHER_H_

#include <map>
#include <set>
#include <string>

#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "base/process/process_handle.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_platform_file.h"
#include "ipc/ipc_sender.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/trusted/ppb_browser_font_trusted.h"
#include "ppapi/proxy/connection.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_proxy_delegate.h"

struct PP_BrowserFont_Trusted_Description;

namespace base {
class MessageLoopProxy;
class WaitableEvent;
}  // namespace base

namespace IPC {
class Message;
class SyncChannel;
}  // namespace IPC

namespace ppapi {

struct PpapiNaClPluginArgs;
struct Preferences;

// This class manages communication between the plugin and the browser, and
// manages the PluginDispatcher instances for communication between the plugin
// and the renderer.
class PpapiDispatcher : public proxy::PluginDispatcher::PluginDelegate,
                        public proxy::PluginProxyDelegate,
                        public IPC::Listener,
                        public IPC::Sender {
 public:
  PpapiDispatcher(scoped_refptr<base::MessageLoopProxy> io_loop,
                  base::WaitableEvent* shutdown_event,
                  int browser_ipc_fd,
                  int renderer_ipc_fd);

  // PluginDispatcher::PluginDelegate implementation.
  virtual base::MessageLoopProxy* GetIPCMessageLoop() override;
  virtual base::WaitableEvent* GetShutdownEvent() override;
  virtual IPC::PlatformFileForTransit ShareHandleWithRemote(
      base::PlatformFile handle,
      base::ProcessId peer_pid,
      bool should_close_source) override;
  virtual std::set<PP_Instance>* GetGloballySeenInstanceIDSet() override;
  virtual uint32 Register(
      proxy::PluginDispatcher* plugin_dispatcher) override;
  virtual void Unregister(uint32 plugin_dispatcher_id) override;

  // PluginProxyDelegate implementation.
  virtual IPC::Sender* GetBrowserSender() override;
  virtual std::string GetUILanguage() override;
  virtual void PreCacheFont(const void* logfontw) override;
  virtual void SetActiveURL(const std::string& url) override;
  virtual PP_Resource CreateBrowserFont(
      proxy::Connection connection,
      PP_Instance instance,
      const PP_BrowserFont_Trusted_Description& desc,
      const Preferences& prefs) override;

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) override;
  virtual void OnChannelError() override;

  // IPC::Sender implementation
  virtual bool Send(IPC::Message* message) override;

 private:
  void OnMsgInitializeNaClDispatcher(const PpapiNaClPluginArgs& args);
  void OnPluginDispatcherMessageReceived(const IPC::Message& msg);

  std::set<PP_Instance> instances_;
  std::map<uint32, proxy::PluginDispatcher*> plugin_dispatchers_;
  uint32 next_plugin_dispatcher_id_;

  scoped_refptr<base::MessageLoopProxy> message_loop_;
  base::WaitableEvent* shutdown_event_;
  int renderer_ipc_fd_;
  scoped_ptr<IPC::SyncChannel> channel_;
};

}  // namespace ppapi

#endif  // PPAPI_NACL_IRT_PPAPI_DISPATCHER_H_
