// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppapi_proxy_test.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace pp {
namespace proxy {

namespace {

ProxyTestBase* current_test = NULL;

const void* MockGetInterface(const char* name) {
  if (!current_test) {
    NOTREACHED();
    return NULL;
  }
  return current_test->GetInterface(name);
}

int32_t MockInitModule(PP_Module, Dispatcher::GetInterfaceFunc) {
  return PP_OK;
}

void MockShutdownModuleFunc() {
}

}  // namespace

// ProxyTestBase ---------------------------------------------------------------

ProxyTestBase::ProxyTestBase() : pp_module_(0x98765), pp_instance_(0x12345) {
  DCHECK(!current_test);
  current_test = this;
}

ProxyTestBase::~ProxyTestBase() {
  DCHECK(current_test == this);
  current_test = NULL;
}

const void* ProxyTestBase::GetInterface(const char* name) {
  return registered_interfaces_[name];
}

void ProxyTestBase::RegisterTestInterface(const char* name,
                                          const void* interface) {
  registered_interfaces_[name] = interface;
}

bool ProxyTestBase::SupportsInterface(const char* name) {
  sink().ClearMessages();

  // IPC doesn't actually write to this when we send a message manually
  // not actually using IPC.
  bool unused_result = false;
  PpapiMsg_SupportsInterface msg(name, &unused_result);
  GetDispatcher()->OnMessageReceived(msg);

  const IPC::Message* reply_msg =
      sink().GetUniqueMessageMatching(IPC_REPLY_ID);
  EXPECT_TRUE(reply_msg);
  if (!reply_msg)
    return false;

  TupleTypes<PpapiMsg_SupportsInterface::ReplyParam>::ValueTuple reply_data;
  EXPECT_TRUE(PpapiMsg_SupportsInterface::ReadReplyParam(
      reply_msg, &reply_data));

  sink().ClearMessages();
  return reply_data.a;
}

// PluginProxyTest -------------------------------------------------------------

PluginProxyTest::PluginProxyTest() {
}

PluginProxyTest::~PluginProxyTest() {
}

Dispatcher* PluginProxyTest::GetDispatcher() {
  return plugin_dispatcher_.get();
}

void PluginProxyTest::SetUp() {
  // These must be first since the dispatcher set-up uses them.
  PluginResourceTracker::SetInstanceForTest(&resource_tracker_);
  PluginVarTracker::SetInstanceForTest(&var_tracker_);

  plugin_dispatcher_.reset(new PluginDispatcher(
      base::Process::Current().handle(),
      &MockGetInterface));
  plugin_dispatcher_->InitWithTestSink(&sink());
  plugin_dispatcher_->DidCreateInstance(pp_instance());
}

void PluginProxyTest::TearDown() {
  plugin_dispatcher_->DidDestroyInstance(pp_instance());
  plugin_dispatcher_.reset();

  PluginVarTracker::SetInstanceForTest(NULL);
  PluginResourceTracker::SetInstanceForTest(NULL);
}

// HostProxyTest ---------------------------------------------------------------

HostProxyTest::HostProxyTest() {
}

HostProxyTest::~HostProxyTest() {
}

Dispatcher* HostProxyTest::GetDispatcher() {
  return host_dispatcher_.get();
}

void HostProxyTest::SetUp() {
  host_dispatcher_.reset(new HostDispatcher(
      base::Process::Current().handle(),
      pp_module(),
      &MockGetInterface));
  host_dispatcher_->InitWithTestSink(&sink());
  HostDispatcher::SetForInstance(pp_instance(), host_dispatcher_.get());
}

void HostProxyTest::TearDown() {
  HostDispatcher::RemoveForInstance(pp_instance());
  host_dispatcher_.reset();
}

}  // namespace proxy
}  // namespace pp
