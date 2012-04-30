// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PLUGIN_GLOBALS_H_
#define PPAPI_PROXY_PLUGIN_GLOBALS_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_local_storage.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/plugin_var_tracker.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/callback_tracker.h"
#include "ppapi/shared_impl/ppapi_globals.h"

namespace ppapi {
namespace proxy {

class PluginProxyDelegate;

class PPAPI_PROXY_EXPORT PluginGlobals : public PpapiGlobals {
 public:
  PluginGlobals();
  PluginGlobals(PpapiGlobals::ForTest);
  virtual ~PluginGlobals();

  // Getter for the global singleton. Generally, you should use
  // PpapiGlobals::Get() when possible. Use this only when you need some
  // plugin-specific functionality.
  inline static PluginGlobals* Get() {
    DCHECK(PpapiGlobals::Get()->IsPluginGlobals());
    return static_cast<PluginGlobals*>(PpapiGlobals::Get());
  }

  // PpapiGlobals implementation.
  virtual ResourceTracker* GetResourceTracker() OVERRIDE;
  virtual VarTracker* GetVarTracker() OVERRIDE;
  virtual CallbackTracker* GetCallbackTrackerForInstance(
      PP_Instance instance) OVERRIDE;
  virtual thunk::PPB_Instance_API* GetInstanceAPI(
      PP_Instance instance) OVERRIDE;
  virtual thunk::ResourceCreationAPI* GetResourceCreationAPI(
      PP_Instance instance) OVERRIDE;
  virtual PP_Module GetModuleForInstance(PP_Instance instance) OVERRIDE;
  virtual std::string GetCmdLine() OVERRIDE;
  virtual void PreCacheFontForFlash(const void* logfontw) OVERRIDE;
  virtual base::Lock* GetProxyLock() OVERRIDE;
  virtual void LogWithSource(PP_Instance instance,
                             PP_LogLevel_Dev level,
                             const std::string& source,
                             const std::string& value) OVERRIDE;
  virtual void BroadcastLogWithSource(PP_Module module,
                                      PP_LogLevel_Dev level,
                                      const std::string& source,
                                      const std::string& value) OVERRIDE;

  // Getters for the plugin-specific versions.
  PluginResourceTracker* plugin_resource_tracker() {
    return &plugin_resource_tracker_;
  }
  PluginVarTracker* plugin_var_tracker() {
    return &plugin_var_tracker_;
  }

  // The embedder should call set_proxy_delegate during startup.
  PluginProxyDelegate* plugin_proxy_delegate() {
    return plugin_proxy_delegate_;
  }
  void set_plugin_proxy_delegate(PluginProxyDelegate* d) {
    plugin_proxy_delegate_ = d;
  }

  // Returns the TLS slot that holds the message loop TLS.
  //
  // If we end up needing more TLS storage for more stuff, we should probably
  // have a struct in here for the different items.
  base::ThreadLocalStorage::Slot* msg_loop_slot() {
    return msg_loop_slot_.get();
  }

  // Sets the message loop slot, takes ownership of the given heap-alloated
  // pointer.
  void set_msg_loop_slot(base::ThreadLocalStorage::Slot* slot) {
    msg_loop_slot_.reset(slot);
  }

  // The embedder should call this function when the name of the plugin module
  // is known. This will be used for error logging.
  void set_plugin_name(const std::string& name) { plugin_name_ = name; }

  // The embedder should call this function when the command line is known.
  void set_command_line(const std::string& c) { command_line_ = c; }

 private:
  // PpapiGlobals overrides.
  virtual bool IsPluginGlobals() const OVERRIDE;

  static PluginGlobals* plugin_globals_;

  PluginProxyDelegate* plugin_proxy_delegate_;
  PluginResourceTracker plugin_resource_tracker_;
  PluginVarTracker plugin_var_tracker_;
  scoped_refptr<CallbackTracker> callback_tracker_;
  base::Lock proxy_lock_;

  scoped_ptr<base::ThreadLocalStorage::Slot> msg_loop_slot_;

  // Name of the plugin used for error logging. This will be empty until
  // set_plugin_name is called.
  std::string plugin_name_;

  // Command line for the plugin. This will be empty until set_command_line is
  // called.
  std::string command_line_;

  DISALLOW_COPY_AND_ASSIGN(PluginGlobals);
};

}  // namespace proxy
}  // namespace ppapi

#endif   // PPAPI_PROXY_PLUGIN_GLOBALS_H_
