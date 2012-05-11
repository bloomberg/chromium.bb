// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/message_loop.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "ipc/ipc_test_sink.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/plugin_proxy_delegate.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/plugin_var_tracker.h"
#include "ppapi/shared_impl/test_globals.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ppapi {
namespace proxy {

// Base class for plugin and host test harnesses. Tests will not use this
// directly. Instead, use the PluginProxyTest, HostProxyTest, or TwoWayTest.
class ProxyTestHarnessBase {
 public:
  ProxyTestHarnessBase();
  virtual ~ProxyTestHarnessBase();

  PP_Module pp_module() const { return pp_module_; }
  PP_Instance pp_instance() const { return pp_instance_; }
  IPC::TestSink& sink() { return sink_; }

  virtual PpapiGlobals* GetGlobals() = 0;
  // Returns either the plugin or host dispatcher, depending on the test.
  virtual Dispatcher* GetDispatcher() = 0;

  // Set up the harness using an IPC::TestSink to capture messages.
  virtual void SetUpHarness() = 0;

  // Set up the harness using a real IPC channel.
  virtual void SetUpHarnessWithChannel(const IPC::ChannelHandle& channel_handle,
                                       base::MessageLoopProxy* ipc_message_loop,
                                       base::WaitableEvent* shutdown_event,
                                       bool is_client) = 0;

  virtual void TearDownHarness() = 0;

  // Implementation of GetInterface for the dispatcher. This will
  // return NULL for all interfaces unless one is registered by calling
  // RegisterTestInterface();
  const void* GetInterface(const char* name);

  // Allows the test to specify an interface implementation for a given
  // interface name. This will be returned when any of the proxy logic
  // requests a local interface.
  void RegisterTestInterface(const char* name, const void* test_interface);

  // Sends a "supports interface" message to the current dispatcher and returns
  // true if it's supported. This is just for the convenience of tests.
  bool SupportsInterface(const char* name);

 private:
  // Destination for IPC messages sent by the test.
  IPC::TestSink sink_;

  // The module and instance ID associated with the plugin dispatcher.
  PP_Module pp_module_;
  PP_Instance pp_instance_;

  // Stores the data for GetInterface/RegisterTestInterface.
  std::map<std::string, const void*> registered_interfaces_;
};

// Test harness for the plugin side of the proxy.
class PluginProxyTestHarness : public ProxyTestHarnessBase {
 public:
  PluginProxyTestHarness();
  virtual ~PluginProxyTestHarness();

  PluginDispatcher* plugin_dispatcher() { return plugin_dispatcher_.get(); }
  PluginResourceTracker& resource_tracker() {
    return *plugin_globals_.plugin_resource_tracker();
  }
  PluginVarTracker& var_tracker() {
    return *plugin_globals_.plugin_var_tracker();
  }

  // ProxyTestHarnessBase implementation.
  virtual PpapiGlobals* GetGlobals() { return &plugin_globals_; }
  virtual Dispatcher* GetDispatcher();
  virtual void SetUpHarness();
  virtual void SetUpHarnessWithChannel(const IPC::ChannelHandle& channel_handle,
                                       base::MessageLoopProxy* ipc_message_loop,
                                       base::WaitableEvent* shutdown_event,
                                       bool is_client);
  virtual void TearDownHarness();

  class PluginDelegateMock : public PluginDispatcher::PluginDelegate,
                             public PluginProxyDelegate {
   public:
    PluginDelegateMock() : ipc_message_loop_(NULL), shutdown_event_() {}
    virtual ~PluginDelegateMock() {}

    void Init(base::MessageLoopProxy* ipc_message_loop,
              base::WaitableEvent* shutdown_event) {
      ipc_message_loop_ = ipc_message_loop;
      shutdown_event_ = shutdown_event;
    }

    // ProxyChannel::Delegate implementation.
    virtual base::MessageLoopProxy* GetIPCMessageLoop() OVERRIDE;
    virtual base::WaitableEvent* GetShutdownEvent() OVERRIDE;
    virtual IPC::PlatformFileForTransit ShareHandleWithRemote(
        base::PlatformFile handle,
        const IPC::SyncChannel& /* channel */,
        bool should_close_source) OVERRIDE;

    // PluginDispatcher::PluginDelegate implementation.
    virtual std::set<PP_Instance>* GetGloballySeenInstanceIDSet() OVERRIDE;
    virtual uint32 Register(PluginDispatcher* plugin_dispatcher) OVERRIDE;
    virtual void Unregister(uint32 plugin_dispatcher_id) OVERRIDE;

    // PluginPepperDelegate implementation.
    virtual bool SendToBrowser(IPC::Message* msg) OVERRIDE;
    virtual void PreCacheFont(const void* logfontw) OVERRIDE;

   private:
    base::MessageLoopProxy* ipc_message_loop_;  // Weak
    base::WaitableEvent* shutdown_event_;  // Weak
    std::set<PP_Instance> instance_id_set_;

    DISALLOW_COPY_AND_ASSIGN(PluginDelegateMock);
  };

 private:
  PluginGlobals plugin_globals_;

  scoped_ptr<PluginDispatcher> plugin_dispatcher_;
  PluginDelegateMock plugin_delegate_mock_;
};

class PluginProxyTest : public PluginProxyTestHarness, public testing::Test {
 public:
  PluginProxyTest();
  virtual ~PluginProxyTest();

  // testing::Test implementation.
  virtual void SetUp();
  virtual void TearDown();
 private:
  MessageLoop message_loop_;
};

class HostProxyTestHarness : public ProxyTestHarnessBase {
 public:
  HostProxyTestHarness();
  virtual ~HostProxyTestHarness();

  HostDispatcher* host_dispatcher() { return host_dispatcher_.get(); }
  ResourceTracker& resource_tracker() {
    return *host_globals_.GetResourceTracker();
  }
  VarTracker& var_tracker() {
    return *host_globals_.GetVarTracker();
  }

  // ProxyTestBase implementation.
  virtual PpapiGlobals* GetGlobals() { return &host_globals_; }
  virtual Dispatcher* GetDispatcher();
  virtual void SetUpHarness();
  virtual void SetUpHarnessWithChannel(const IPC::ChannelHandle& channel_handle,
                                       base::MessageLoopProxy* ipc_message_loop,
                                       base::WaitableEvent* shutdown_event,
                                       bool is_client);
  virtual void TearDownHarness();

  class DelegateMock : public ProxyChannel::Delegate {
   public:
    DelegateMock() : ipc_message_loop_(NULL), shutdown_event_(NULL) {
    }
    virtual ~DelegateMock() {}

    void Init(base::MessageLoopProxy* ipc_message_loop,
              base::WaitableEvent* shutdown_event) {
      ipc_message_loop_ = ipc_message_loop;
      shutdown_event_ = shutdown_event;
    }

    // ProxyChannel::Delegate implementation.
    virtual base::MessageLoopProxy* GetIPCMessageLoop();
    virtual base::WaitableEvent* GetShutdownEvent();
    virtual IPC::PlatformFileForTransit ShareHandleWithRemote(
        base::PlatformFile handle,
        const IPC::SyncChannel& /* channel */,
        bool should_close_source) OVERRIDE;

   private:
    base::MessageLoopProxy* ipc_message_loop_;  // Weak
    base::WaitableEvent* shutdown_event_;  // Weak

    DISALLOW_COPY_AND_ASSIGN(DelegateMock);
  };

 private:
  class MockSyncMessageStatusReceiver;

  ppapi::TestGlobals host_globals_;
  scoped_ptr<HostDispatcher> host_dispatcher_;
  DelegateMock delegate_mock_;

  scoped_ptr<MockSyncMessageStatusReceiver> status_receiver_;
};

class HostProxyTest : public HostProxyTestHarness, public testing::Test {
 public:
  HostProxyTest();
  virtual ~HostProxyTest();

  // testing::Test implementation.
  virtual void SetUp();
  virtual void TearDown();
 private:
  MessageLoop message_loop_;
};

// Use this base class to test both sides of a proxy.
class TwoWayTest : public testing::Test {
 public:
  enum TwoWayTestMode {
    TEST_PPP_INTERFACE,
    TEST_PPB_INTERFACE
  };
  TwoWayTest(TwoWayTestMode test_mode);
  virtual ~TwoWayTest();

  HostProxyTestHarness& host() { return host_; }
  PluginProxyTestHarness& plugin() { return plugin_; }
  PP_Module pp_module() const { return host_.pp_module(); }
  PP_Instance pp_instance() const { return host_.pp_instance(); }
  TwoWayTestMode test_mode() { return test_mode_; }

  // testing::Test implementation.
  virtual void SetUp();
  virtual void TearDown();

 private:
  TwoWayTestMode test_mode_;
  HostProxyTestHarness host_;
  PluginProxyTestHarness plugin_;
  // In order to use sync IPC, we need to have an IO thread.
  base::Thread io_thread_;
  // The plugin side of the proxy runs on its own thread.
  base::Thread plugin_thread_;
  // The message loop for the main (host) thread.
  MessageLoop message_loop_;

  // Aliases for the host and plugin harnesses; if we're testing a PPP
  // interface, remote_harness will point to plugin_, and local_harness
  // will point to host_.  This makes it convenient when we're starting and
  // stopping the harnesses.
  ProxyTestHarnessBase* remote_harness_;
  ProxyTestHarnessBase* local_harness_;

  base::WaitableEvent channel_created_;
  base::WaitableEvent shutdown_event_;
};

}  // namespace proxy
}  // namespace ppapi
