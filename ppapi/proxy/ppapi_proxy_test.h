// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "ipc/ipc_test_sink.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/plugin_var_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace pp {
namespace proxy {

// Test harness for the plugin side of the proxy.
class PluginProxyTest : public testing::Test {
 public:
  PluginProxyTest();
  ~PluginProxyTest();

  virtual void SetUp();
  virtual void TearDown();

  // The instance ID associated with the plugin dispatcher.
  PP_Instance pp_instance() const { return pp_instance_; }

  PluginDispatcher* plugin_dispatcher() { return plugin_dispatcher_.get(); }

  PluginResourceTracker& resource_tracker() { return resource_tracker_; }
  PluginVarTracker& var_tracker() { return var_tracker_; }

  IPC::TestSink& sink() { return sink_; }

 private:
  PluginResourceTracker resource_tracker_;
  PluginVarTracker var_tracker_;

  PP_Instance pp_instance_;
  scoped_ptr<PluginDispatcher> plugin_dispatcher_;

  IPC::TestSink sink_;
};

}  // namespace proxy
}  // namespace pp
