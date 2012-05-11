// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppapi_proxy_test.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/message_loop_proxy.h"
#include "base/observer_list.h"
#include "ipc/ipc_sync_channel.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_proxy_private.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace ppapi {
namespace proxy {

namespace {
// HostDispatcher requires a PPB_Proxy_Private, so we always provide a fallback
// do-nothing implementation.
void PluginCrashed(PP_Module module) {
  NOTREACHED();
};

PP_Instance GetInstanceForResource(PP_Resource resource) {
  // If a test relies on this, we need to implement it.
  NOTREACHED();
  return 0;
}

void SetReserveInstanceIDCallback(PP_Module module,
                                  PP_Bool (*is_seen)(PP_Module, PP_Instance)) {
  // This function gets called in HostDispatcher's constructor.  We simply don't
  // worry about Instance uniqueness in tests, so we can ignore the call.
}

int32_t GetURLLoaderBufferedBytes(PP_Resource url_loader) {
  NOTREACHED();
  return 0;
}

void AddRefModule(PP_Module module) {}
void ReleaseModule(PP_Module module) {}
PP_Bool IsInModuleDestructor(PP_Module module) { return PP_FALSE; }

PPB_Proxy_Private ppb_proxy_private = {
  &PluginCrashed,
  &GetInstanceForResource,
  &SetReserveInstanceIDCallback,
  &GetURLLoaderBufferedBytes,
  &AddRefModule,
  &ReleaseModule,
  &IsInModuleDestructor
};

// We allow multiple harnesses at a time to respond to 'GetInterface' calls.
// We assume that only 1 harness's GetInterface function will ever support a
// given interface name. In practice, there will either be only 1 GetInterface
// handler (for PluginProxyTest or HostProxyTest), or there will be only 2
// GetInterface handlers (for TwoWayTest).  In the latter case, one handler is
// for the PluginProxyTestHarness and should only respond for PPP interfaces,
// and the other handler is for the HostProxyTestHarness which should only
// ever respond for PPB interfaces.
ObserverList<ProxyTestHarnessBase> get_interface_handlers_;

const void* MockGetInterface(const char* name) {
  ObserverList<ProxyTestHarnessBase>::Iterator it =
      get_interface_handlers_;
  while (ProxyTestHarnessBase* observer = it.GetNext()) {
    const void* interface = observer->GetInterface(name);
    if (interface)
      return interface;
  }
  if (strcmp(name, PPB_PROXY_PRIVATE_INTERFACE) == 0)
    return &ppb_proxy_private;
  return NULL;
}

void SetUpRemoteHarness(ProxyTestHarnessBase* harness,
                        const IPC::ChannelHandle& handle,
                        base::MessageLoopProxy* ipc_message_loop_proxy,
                        base::WaitableEvent* shutdown_event,
                        base::WaitableEvent* harness_set_up) {
  harness->SetUpHarnessWithChannel(handle, ipc_message_loop_proxy,
                                   shutdown_event, false);
  harness_set_up->Signal();
}

void TearDownRemoteHarness(ProxyTestHarnessBase* harness,
                           base::WaitableEvent* harness_torn_down) {
  harness->TearDownHarness();
  harness_torn_down->Signal();
}

}  // namespace

// ProxyTestHarnessBase --------------------------------------------------------

ProxyTestHarnessBase::ProxyTestHarnessBase() : pp_module_(0x98765),
                                               pp_instance_(0x12345) {
  get_interface_handlers_.AddObserver(this);
}

ProxyTestHarnessBase::~ProxyTestHarnessBase() {
  get_interface_handlers_.RemoveObserver(this);
}

const void* ProxyTestHarnessBase::GetInterface(const char* name) {
  return registered_interfaces_[name];
}

void ProxyTestHarnessBase::RegisterTestInterface(const char* name,
                                                 const void* test_interface) {
  registered_interfaces_[name] = test_interface;
}

bool ProxyTestHarnessBase::SupportsInterface(const char* name) {
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

// PluginProxyTestHarness ------------------------------------------------------

PluginProxyTestHarness::PluginProxyTestHarness()
    : plugin_globals_(PpapiGlobals::ForTest()) {
}

PluginProxyTestHarness::~PluginProxyTestHarness() {
}

Dispatcher* PluginProxyTestHarness::GetDispatcher() {
  return plugin_dispatcher_.get();
}

void PluginProxyTestHarness::SetUpHarness() {
  // These must be first since the dispatcher set-up uses them.
  PpapiGlobals::SetPpapiGlobalsOnThreadForTest(GetGlobals());
  resource_tracker().DidCreateInstance(pp_instance());

  plugin_dispatcher_.reset(new PluginDispatcher(
      &MockGetInterface,
      false));
  plugin_dispatcher_->InitWithTestSink(&sink());
  plugin_dispatcher_->DidCreateInstance(pp_instance());
}

void PluginProxyTestHarness::SetUpHarnessWithChannel(
    const IPC::ChannelHandle& channel_handle,
    base::MessageLoopProxy* ipc_message_loop,
    base::WaitableEvent* shutdown_event,
    bool is_client) {
  // These must be first since the dispatcher set-up uses them.
  PpapiGlobals::SetPpapiGlobalsOnThreadForTest(GetGlobals());
  resource_tracker().DidCreateInstance(pp_instance());
  plugin_delegate_mock_.Init(ipc_message_loop, shutdown_event);

  plugin_dispatcher_.reset(new PluginDispatcher(
      &MockGetInterface,
      false));
  plugin_dispatcher_->InitPluginWithChannel(&plugin_delegate_mock_,
                                            channel_handle,
                                            is_client);
  plugin_dispatcher_->DidCreateInstance(pp_instance());
}

void PluginProxyTestHarness::TearDownHarness() {
  plugin_dispatcher_->DidDestroyInstance(pp_instance());
  plugin_dispatcher_.reset();

  resource_tracker().DidDeleteInstance(pp_instance());
}

base::MessageLoopProxy*
PluginProxyTestHarness::PluginDelegateMock::GetIPCMessageLoop() {
  return ipc_message_loop_;
}

base::WaitableEvent*
PluginProxyTestHarness::PluginDelegateMock::GetShutdownEvent() {
  return shutdown_event_;
}

IPC::PlatformFileForTransit
PluginProxyTestHarness::PluginDelegateMock::ShareHandleWithRemote(
    base::PlatformFile handle,
    const IPC::SyncChannel& /* channel */,
    bool should_close_source) {
  return IPC::GetFileHandleForProcess(handle,
                                      base::Process::Current().handle(),
                                      should_close_source);
}

std::set<PP_Instance>*
PluginProxyTestHarness::PluginDelegateMock::GetGloballySeenInstanceIDSet() {
  return &instance_id_set_;
}

uint32 PluginProxyTestHarness::PluginDelegateMock::Register(
    PluginDispatcher* plugin_dispatcher) {
  return 0;
}

void PluginProxyTestHarness::PluginDelegateMock::Unregister(
    uint32 plugin_dispatcher_id) {
}

bool PluginProxyTestHarness::PluginDelegateMock::SendToBrowser(
    IPC::Message* msg) {
  NOTREACHED();
  return false;
}

void PluginProxyTestHarness::PluginDelegateMock::PreCacheFont(
    const void* logfontw) {
}

// PluginProxyTest -------------------------------------------------------------

PluginProxyTest::PluginProxyTest() {
}

PluginProxyTest::~PluginProxyTest() {
}

void PluginProxyTest::SetUp() {
  SetUpHarness();
}

void PluginProxyTest::TearDown() {
  TearDownHarness();
}

// HostProxyTestHarness --------------------------------------------------------

class HostProxyTestHarness::MockSyncMessageStatusReceiver
    : public HostDispatcher::SyncMessageStatusReceiver {
 public:
  virtual void BeginBlockOnSyncMessage() OVERRIDE {}
  virtual void EndBlockOnSyncMessage() OVERRIDE {}
};

HostProxyTestHarness::HostProxyTestHarness()
    : host_globals_(PpapiGlobals::ForTest()),
      status_receiver_(new MockSyncMessageStatusReceiver) {
}

HostProxyTestHarness::~HostProxyTestHarness() {
}

Dispatcher* HostProxyTestHarness::GetDispatcher() {
  return host_dispatcher_.get();
}

void HostProxyTestHarness::SetUpHarness() {
  // These must be first since the dispatcher set-up uses them.
  PpapiGlobals::SetPpapiGlobalsOnThreadForTest(GetGlobals());
  host_dispatcher_.reset(new HostDispatcher(
      pp_module(),
      &MockGetInterface,
      status_receiver_.get()));
  host_dispatcher_->InitWithTestSink(&sink());
  HostDispatcher::SetForInstance(pp_instance(), host_dispatcher_.get());
}

void HostProxyTestHarness::SetUpHarnessWithChannel(
    const IPC::ChannelHandle& channel_handle,
    base::MessageLoopProxy* ipc_message_loop,
    base::WaitableEvent* shutdown_event,
    bool is_client) {
  // These must be first since the dispatcher set-up uses them.
  PpapiGlobals::SetPpapiGlobalsOnThreadForTest(GetGlobals());
  delegate_mock_.Init(ipc_message_loop, shutdown_event);

  host_dispatcher_.reset(new HostDispatcher(
      pp_module(),
      &MockGetInterface,
      status_receiver_.get()));
  ppapi::Preferences preferences;
  host_dispatcher_->InitHostWithChannel(&delegate_mock_, channel_handle,
                                        is_client, preferences);
  HostDispatcher::SetForInstance(pp_instance(), host_dispatcher_.get());
}

void HostProxyTestHarness::TearDownHarness() {
  HostDispatcher::RemoveForInstance(pp_instance());
  host_dispatcher_.reset();
}

base::MessageLoopProxy*
HostProxyTestHarness::DelegateMock::GetIPCMessageLoop() {
  return ipc_message_loop_;
}

base::WaitableEvent* HostProxyTestHarness::DelegateMock::GetShutdownEvent() {
  return shutdown_event_;
}

IPC::PlatformFileForTransit
HostProxyTestHarness::DelegateMock::ShareHandleWithRemote(
    base::PlatformFile handle,
    const IPC::SyncChannel& /* channel */,
    bool should_close_source) {
  return IPC::GetFileHandleForProcess(handle,
                                      base::Process::Current().handle(),
                                      should_close_source);
}


// HostProxyTest ---------------------------------------------------------------

HostProxyTest::HostProxyTest() {
}

HostProxyTest::~HostProxyTest() {
}

void HostProxyTest::SetUp() {
  SetUpHarness();
}

void HostProxyTest::TearDown() {
  TearDownHarness();
}

// TwoWayTest ---------------------------------------------------------------

TwoWayTest::TwoWayTest(TwoWayTest::TwoWayTestMode test_mode)
    : test_mode_(test_mode),
      io_thread_("TwoWayTest_IOThread"),
      plugin_thread_("TwoWayTest_PluginThread"),
      remote_harness_(NULL),
      local_harness_(NULL),
      channel_created_(true, false),
      shutdown_event_(true, false) {
  if (test_mode == TEST_PPP_INTERFACE) {
    remote_harness_ = &plugin_;
    local_harness_ = &host_;
  } else {
    remote_harness_ = &host_;
    local_harness_ = &plugin_;
  }
}

TwoWayTest::~TwoWayTest() {
  shutdown_event_.Signal();
}

void TwoWayTest::SetUp() {
  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  io_thread_.StartWithOptions(options);
  plugin_thread_.Start();

  IPC::ChannelHandle handle;
  handle.name = "TwoWayTestChannel";
  base::WaitableEvent remote_harness_set_up(true, false);
  plugin_thread_.message_loop_proxy()->PostTask(
      FROM_HERE,
      base::Bind(&SetUpRemoteHarness,
                 remote_harness_,
                 handle,
                 io_thread_.message_loop_proxy(),
                 &shutdown_event_,
                 &remote_harness_set_up));
  remote_harness_set_up.Wait();
  local_harness_->SetUpHarnessWithChannel(handle,
                                          io_thread_.message_loop_proxy(),
                                          &shutdown_event_,
                                          true);  // is_client
}

void TwoWayTest::TearDown() {
  base::WaitableEvent remote_harness_torn_down(true, false);
  plugin_thread_.message_loop_proxy()->PostTask(
      FROM_HERE,
      base::Bind(&TearDownRemoteHarness,
                 remote_harness_,
                 &remote_harness_torn_down));
  remote_harness_torn_down.Wait();

  local_harness_->TearDownHarness();

  io_thread_.Stop();
}


}  // namespace proxy
}  // namespace ppapi
