// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/mojo/src/mojo/edk/embedder/embedder.h"

#include <utility>

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/task_runner.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/embedder_internal.h"
#include "mojo/edk/embedder/process_delegate.h"
#include "mojo/edk/system/core.h"
#include "third_party/mojo/src/mojo/edk/embedder/embedder_internal.h"
#include "third_party/mojo/src/mojo/edk/embedder/master_process_delegate.h"
#include "third_party/mojo/src/mojo/edk/embedder/process_delegate.h"
#include "third_party/mojo/src/mojo/edk/embedder/simple_platform_support.h"
#include "third_party/mojo/src/mojo/edk/embedder/slave_process_delegate.h"
#include "third_party/mojo/src/mojo/edk/system/channel.h"
#include "third_party/mojo/src/mojo/edk/system/channel_manager.h"
#include "third_party/mojo/src/mojo/edk/system/configuration.h"
#include "third_party/mojo/src/mojo/edk/system/core.h"
#include "third_party/mojo/src/mojo/edk/system/ipc_support.h"
#include "third_party/mojo/src/mojo/edk/system/message_pipe_dispatcher.h"
#include "third_party/mojo/src/mojo/edk/system/platform_handle_dispatcher.h"
#include "third_party/mojo/src/mojo/edk/system/raw_channel.h"

namespace mojo {
namespace embedder {

namespace internal {

// Declared in embedder_internal.h.
PlatformSupport* g_platform_support = nullptr;
system::Core* g_core = nullptr;
system::IPCSupport* g_ipc_support = nullptr;

}  // namespace internal

namespace {

bool UseNewEDK() {
  static bool checked = false;
  static bool use_new = false;
  if (!checked) {
    use_new = base::CommandLine::ForCurrentProcess()->HasSwitch("use-new-edk");
    checked = true;
  }
  return use_new;
}

// TODO(use_chrome_edk): temporary wrapper.
class NewEDKProcessDelegate : public mojo::edk::ProcessDelegate {
 public:
  NewEDKProcessDelegate(mojo::embedder::ProcessDelegate* passed_in_delegate)
      : passed_in_delegate_(passed_in_delegate) {}
  ~NewEDKProcessDelegate() {}

  void OnShutdownComplete() {
    passed_in_delegate_->OnShutdownComplete();
    delete this;
  }

 private:
  mojo::embedder::ProcessDelegate* passed_in_delegate_;
};

NewEDKProcessDelegate* g_wrapper_process_delegate = nullptr;

// TODO(vtl): For now, we need this to be thread-safe (since theoretically we
// currently support multiple channel creation threads -- possibly one per
// channel). Eventually, we won't need it to be thread-safe (we'll require a
// single I/O thread), and eventually we won't need it at all. Remember to
// remove the base/atomicops.h include.
system::ChannelId MakeChannelId() {
  // Note that |AtomicWord| is signed.
  static base::subtle::AtomicWord counter = 0;

  base::subtle::AtomicWord new_counter_value =
      base::subtle::NoBarrier_AtomicIncrement(&counter, 1);
  // Don't allow the counter to wrap. Note that any (strictly) positive value is
  // a valid |ChannelId| (and |NoBarrier_AtomicIncrement()| returns the value
  // post-increment).
  CHECK_GT(new_counter_value, 0);
  // Use "negative" values for these IDs, so that we'll also be able to use
  // "positive" "process identifiers" (see connection_manager.h) as IDs (and
  // they won't conflict).
  return static_cast<system::ChannelId>(-new_counter_value);
}

edk::ScopedPlatformHandle CreateEDKHandle(ScopedPlatformHandle handle) {
  return edk::ScopedPlatformHandle(edk::PlatformHandle(
#if defined(OS_WIN)
        handle.release().handle));
#else
        handle.release().fd));
#endif
}

ScopedPlatformHandle CreateHandle(edk::ScopedPlatformHandle handle) {
  return ScopedPlatformHandle(PlatformHandle(handle.release().handle));
}

}  // namespace

void PreInitializeParentProcess() {
  edk::PreInitializeParentProcess();
}

void PreInitializeChildProcess() {
  edk::PreInitializeChildProcess();
}

ScopedPlatformHandle ChildProcessLaunched(base::ProcessHandle child_process) {
  return CreateHandle(edk::ChildProcessLaunched(child_process));
}

void ChildProcessLaunched(base::ProcessHandle child_process,
                          ScopedPlatformHandle server_pipe) {
  return edk::ChildProcessLaunched(child_process,
                                   CreateEDKHandle(std::move(server_pipe)));
}

void SetParentPipeHandle(ScopedPlatformHandle pipe) {
  edk::SetParentPipeHandle(CreateEDKHandle(std::move(pipe)));
}

void SetMaxMessageSize(size_t bytes) {
  system::GetMutableConfiguration()->max_message_num_bytes = bytes;
  // TODO(use_chrome_edk): also set this in the new EDK.
  mojo::edk::SetMaxMessageSize(bytes);
}

void Init() {
  DCHECK(!internal::g_platform_support);
  internal::g_platform_support = new SimplePlatformSupport();

  DCHECK(!internal::g_core);
  internal::g_core = new system::Core(internal::g_platform_support);
  // TODO(use_chrome_edk): also initialize the new EDK.
  mojo::edk::Init();
}

MojoResult AsyncWait(MojoHandle handle,
                     MojoHandleSignals signals,
                     const base::Callback<void(MojoResult)>& callback) {
  if (UseNewEDK())
    return mojo::edk::internal::g_core->AsyncWait(handle, signals, callback);
  return internal::g_core->AsyncWait(handle, signals, callback);
}

MojoResult CreatePlatformHandleWrapper(
    ScopedPlatformHandle platform_handle,
    MojoHandle* platform_handle_wrapper_handle) {
  DCHECK(platform_handle_wrapper_handle);
  if (UseNewEDK()) {
    return mojo::edk::CreatePlatformHandleWrapper(
        CreateEDKHandle(std::move(platform_handle)),
        platform_handle_wrapper_handle);
  }

  scoped_refptr<system::Dispatcher> dispatcher =
      system::PlatformHandleDispatcher::Create(std::move(platform_handle));

  DCHECK(internal::g_core);
  MojoHandle h = internal::g_core->AddDispatcher(dispatcher);
  if (h == MOJO_HANDLE_INVALID) {
    LOG(ERROR) << "Handle table full";
    dispatcher->Close();
    return MOJO_RESULT_RESOURCE_EXHAUSTED;
  }

  *platform_handle_wrapper_handle = h;
  return MOJO_RESULT_OK;
}

MojoResult PassWrappedPlatformHandle(MojoHandle platform_handle_wrapper_handle,
                                     ScopedPlatformHandle* platform_handle) {
  DCHECK(platform_handle);
  if (UseNewEDK()) {
    mojo::edk::ScopedPlatformHandle edk_handle;
    MojoResult rv = mojo::edk::PassWrappedPlatformHandle(
        platform_handle_wrapper_handle, &edk_handle);
    platform_handle->reset(mojo::embedder::PlatformHandle(
        edk_handle.release().handle));
    return rv;
  }

  DCHECK(internal::g_core);
  scoped_refptr<system::Dispatcher> dispatcher(
      internal::g_core->GetDispatcher(platform_handle_wrapper_handle));
  if (!dispatcher)
    return MOJO_RESULT_INVALID_ARGUMENT;

  if (dispatcher->GetType() != system::Dispatcher::Type::PLATFORM_HANDLE)
    return MOJO_RESULT_INVALID_ARGUMENT;

  *platform_handle =
      static_cast<system::PlatformHandleDispatcher*>(dispatcher.get())
          ->PassPlatformHandle();
  return MOJO_RESULT_OK;
}

void InitIPCSupport(ProcessType process_type,
                    ProcessDelegate* process_delegate,
                    scoped_refptr<base::TaskRunner> io_thread_task_runner,
                    ScopedPlatformHandle platform_handle) {
  // |Init()| must have already been called.
  DCHECK(internal::g_core);
  // And not |InitIPCSupport()| (without |ShutdownIPCSupport()|).
  DCHECK(!internal::g_ipc_support);

  internal::g_ipc_support = new system::IPCSupport(
      internal::g_platform_support, process_type, process_delegate,
      io_thread_task_runner, std::move(platform_handle));

  // TODO(use_chrome_edk) at this point the command line to switch to the new
  // EDK might not be set yet. There's no harm in always intializing the new EDK
  // though.
  g_wrapper_process_delegate = new NewEDKProcessDelegate(process_delegate);
  mojo::edk::InitIPCSupport(g_wrapper_process_delegate, io_thread_task_runner);
}

void ShutdownIPCSupportOnIOThread() {
  if (UseNewEDK()) {
    mojo::edk::ShutdownIPCSupportOnIOThread();
    return;
  }

  DCHECK(internal::g_ipc_support);

  internal::g_ipc_support->ShutdownOnIOThread();
  delete internal::g_ipc_support;
  internal::g_ipc_support = nullptr;
  delete g_wrapper_process_delegate;
  g_wrapper_process_delegate = nullptr;
}

void ShutdownIPCSupport() {
  if (UseNewEDK()) {
    mojo::edk::ShutdownIPCSupport();
    return;
  }

  DCHECK(internal::g_ipc_support);

  ProcessDelegate* delegate = internal::g_ipc_support->process_delegate();
  bool ok = internal::g_ipc_support->io_thread_task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&ShutdownIPCSupportOnIOThread),
      base::Bind(&ProcessDelegate::OnShutdownComplete,
                 base::Unretained(delegate)));
  DCHECK(ok);
}

ScopedMessagePipeHandle ConnectToSlave(
    SlaveInfo slave_info,
    ScopedPlatformHandle platform_handle,
    const base::Closure& did_connect_to_slave_callback,
    scoped_refptr<base::TaskRunner> did_connect_to_slave_runner,
    std::string* platform_connection_id,
    ChannelInfo** channel_info) {
  DCHECK(platform_connection_id);
  DCHECK(channel_info);
  DCHECK(internal::g_ipc_support);

  system::ConnectionIdentifier connection_id =
      internal::g_ipc_support->GenerateConnectionIdentifier();
  *platform_connection_id = connection_id.ToString();
  system::ChannelId channel_id = system::kInvalidChannelId;
  scoped_refptr<system::MessagePipeDispatcher> dispatcher =
      internal::g_ipc_support->ConnectToSlave(
          connection_id, slave_info, std::move(platform_handle),
          did_connect_to_slave_callback, std::move(did_connect_to_slave_runner),
          &channel_id);
  *channel_info = new ChannelInfo(channel_id);

  ScopedMessagePipeHandle rv(
      MessagePipeHandle(internal::g_core->AddDispatcher(dispatcher)));
  CHECK(rv.is_valid());
  // TODO(vtl): The |.Pass()| below is only needed due to an MSVS bug; remove it
  // once that's fixed.
  return rv;
}

ScopedMessagePipeHandle ConnectToMaster(
    const std::string& platform_connection_id,
    const base::Closure& did_connect_to_master_callback,
    scoped_refptr<base::TaskRunner> did_connect_to_master_runner,
    ChannelInfo** channel_info) {
  DCHECK(channel_info);
  DCHECK(internal::g_ipc_support);

  bool ok = false;
  system::ConnectionIdentifier connection_id =
      system::ConnectionIdentifier::FromString(platform_connection_id, &ok);
  CHECK(ok);

  system::ChannelId channel_id = system::kInvalidChannelId;
  scoped_refptr<system::MessagePipeDispatcher> dispatcher =
      internal::g_ipc_support->ConnectToMaster(
          connection_id, did_connect_to_master_callback,
          std::move(did_connect_to_master_runner), &channel_id);
  *channel_info = new ChannelInfo(channel_id);

  ScopedMessagePipeHandle rv(
      MessagePipeHandle(internal::g_core->AddDispatcher(dispatcher)));
  CHECK(rv.is_valid());
  // TODO(vtl): The |.Pass()| below is only needed due to an MSVS bug; remove it
  // once that's fixed.
  return rv;
}

// TODO(vtl): Write tests for this.
ScopedMessagePipeHandle CreateChannelOnIOThread(
    ScopedPlatformHandle platform_handle,
    ChannelInfo** channel_info) {
  DCHECK(platform_handle.is_valid());
  DCHECK(channel_info);
  DCHECK(internal::g_ipc_support);

  system::ChannelManager* channel_manager =
      internal::g_ipc_support->channel_manager();

  *channel_info = new ChannelInfo(MakeChannelId());
  scoped_refptr<system::MessagePipeDispatcher> dispatcher =
      channel_manager->CreateChannelOnIOThread((*channel_info)->channel_id,
                                               std::move(platform_handle));

  ScopedMessagePipeHandle rv(
      MessagePipeHandle(internal::g_core->AddDispatcher(dispatcher)));
  CHECK(rv.is_valid());
  // TODO(vtl): The |.Pass()| below is only needed due to an MSVS bug; remove it
  // once that's fixed.
  return rv;
}

ScopedMessagePipeHandle CreateChannel(
    ScopedPlatformHandle platform_handle,
    const base::Callback<void(ChannelInfo*)>& did_create_channel_callback,
    scoped_refptr<base::TaskRunner> did_create_channel_runner) {
  DCHECK(platform_handle.is_valid());
  DCHECK(!did_create_channel_callback.is_null());
  DCHECK(internal::g_ipc_support);

  if (UseNewEDK()) {
    if (!did_create_channel_callback.is_null())
      did_create_channel_callback.Run(nullptr);
    return mojo::edk::CreateMessagePipe(
        CreateEDKHandle(std::move(platform_handle)));
  }

  system::ChannelManager* channel_manager =
      internal::g_ipc_support->channel_manager();

  system::ChannelId channel_id = MakeChannelId();
  scoped_ptr<ChannelInfo> channel_info(new ChannelInfo(channel_id));
  scoped_refptr<system::MessagePipeDispatcher> dispatcher =
      channel_manager->CreateChannel(
          channel_id, std::move(platform_handle),
          base::Bind(did_create_channel_callback,
                     base::Unretained(channel_info.release())),
          did_create_channel_runner);

  ScopedMessagePipeHandle rv(
      MessagePipeHandle(internal::g_core->AddDispatcher(dispatcher)));
  CHECK(rv.is_valid());
  // TODO(vtl): The |.Pass()| below is only needed due to an MSVS bug; remove it
  // once that's fixed.
  return rv;
}

// TODO(vtl): Write tests for this.
void DestroyChannelOnIOThread(ChannelInfo* channel_info) {
  DCHECK(channel_info);
  DCHECK(channel_info->channel_id);
  DCHECK(internal::g_ipc_support);

  system::ChannelManager* channel_manager =
      internal::g_ipc_support->channel_manager();
  channel_manager->ShutdownChannelOnIOThread(channel_info->channel_id);
  delete channel_info;
}

// TODO(vtl): Write tests for this.
void DestroyChannel(
    ChannelInfo* channel_info,
    const base::Closure& did_destroy_channel_callback,
    scoped_refptr<base::TaskRunner> did_destroy_channel_runner) {
  DCHECK(channel_info);
  DCHECK(channel_info->channel_id);
  DCHECK(!did_destroy_channel_callback.is_null());
  DCHECK(internal::g_ipc_support);

  system::ChannelManager* channel_manager =
      internal::g_ipc_support->channel_manager();
  channel_manager->ShutdownChannel(channel_info->channel_id,
                                   did_destroy_channel_callback,
                                   did_destroy_channel_runner);
  delete channel_info;
}

void WillDestroyChannelSoon(ChannelInfo* channel_info) {
  DCHECK(channel_info);
  DCHECK(internal::g_ipc_support);

  system::ChannelManager* channel_manager =
      internal::g_ipc_support->channel_manager();
  channel_manager->WillShutdownChannel(channel_info->channel_id);
}

}  // namespace embedder
}  // namespace mojo
