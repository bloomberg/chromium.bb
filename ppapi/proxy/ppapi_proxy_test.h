// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/scoped_ptr.h"
#include "ipc/ipc_test_sink.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/plugin_var_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace pp {
namespace proxy {

// Base class for plugin and host tests. Tests will not use this directly.
// Instead, use the Plugin or HostProxyTest.
class ProxyTestBase : public testing::Test {
 public:
  ProxyTestBase();
  virtual ~ProxyTestBase();

  PP_Module pp_module() const { return pp_module_; }
  PP_Instance pp_instance() const { return pp_instance_; }
  IPC::TestSink& sink() { return sink_; }

  // Returns either the plugin or host dispatcher, depending on the test.
  virtual Dispatcher* GetDispatcher() = 0;

  // Implementation of GetInterface for the dispatcher. This will
  // return NULL for all interfaces unless one is registered by calling
  // RegisterTestInterface();
  const void* GetInterface(const char* name);

  // Allows the test to specify an interface implementation for a given
  // interface name. This will be returned when any of the proxy logic
  // requests a local interface.
  void RegisterTestInterface(const char* name, const void* interface);

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
class PluginProxyTest : public ProxyTestBase {
 public:
  PluginProxyTest();
  virtual ~PluginProxyTest();

  PluginDispatcher* plugin_dispatcher() { return plugin_dispatcher_.get(); }
  PluginResourceTracker& resource_tracker() { return resource_tracker_; }
  PluginVarTracker& var_tracker() { return var_tracker_; }

  // ProxyTestBase implementation.
  virtual Dispatcher* GetDispatcher();

  // testing::Test implementation.
  virtual void SetUp();
  virtual void TearDown();

 private:
  PluginResourceTracker resource_tracker_;
  PluginVarTracker var_tracker_;
  scoped_ptr<PluginDispatcher> plugin_dispatcher_;
};

class HostProxyTest : public ProxyTestBase {
 public:
  HostProxyTest();
  virtual ~HostProxyTest();

  HostDispatcher* host_dispatcher() { return host_dispatcher_.get(); }

  // ProxyTestBase implementation.
  virtual Dispatcher* GetDispatcher();

  // testing::Test implementation.
  virtual void SetUp();
  virtual void TearDown();

 private:
  scoped_ptr<HostDispatcher> host_dispatcher_;
};

}  // namespace proxy
}  // namespace pp
