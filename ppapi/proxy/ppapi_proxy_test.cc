// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppapi_proxy_test.h"

#include "ppapi/c/pp_errors.h"

namespace pp {
namespace proxy {

namespace {

const void* MockGetInterface(const char*) {
  return NULL;
}

int32_t MockInitModule(PP_Module, Dispatcher::GetInterfaceFunc) {
  return PP_OK;
}

void MockShutdownModuleFunc() {
}

}  // namespace

PluginProxyTest::PluginProxyTest() {
}

PluginProxyTest::~PluginProxyTest() {
}

void PluginProxyTest::SetUp() {
  // These must be first since the dispatcher set-up uses them.
  PluginResourceTracker::SetInstanceForTest(&resource_tracker_);
  PluginVarTracker::SetInstanceForTest(&var_tracker_);

  pp_instance_ = 0x1234;
  plugin_dispatcher_.reset(new PluginDispatcher(
      base::Process::Current().handle(),
      &MockGetInterface,
      &MockInitModule,
      &MockShutdownModuleFunc));
  plugin_dispatcher_->InitWithTestSink(&sink_);
  // When the plugin dispatcher is per-instance, this is the line to use:
  // PluginDispatcher::SetForInstance(pp_instance_, plugin_dispatcher_.get());
  PluginDispatcher::SetGlobal(plugin_dispatcher_.get());
}

void PluginProxyTest::TearDown() {
  PluginDispatcher::SetGlobal(NULL);
  plugin_dispatcher_.reset();

  PluginVarTracker::SetInstanceForTest(NULL);
  PluginResourceTracker::SetInstanceForTest(NULL);
}

}  // namespace proxy
}  // namespace pp
