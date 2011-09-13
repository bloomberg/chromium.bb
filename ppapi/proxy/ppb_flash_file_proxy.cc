// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_flash_file_proxy.h"

#include <map>
#include <set>
#include <vector>

#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "build/build_config.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sync_message.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/private/ppb_flash_file.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/resource.h"

namespace ppapi {
namespace proxy {

namespace {

// Given an error code and a handle result from a Pepper API call, converts to a
// PlatformFileForTransit by sharing with the other side, closing the original
// handle, possibly also updating the error value if an error occurred.
IPC::PlatformFileForTransit PlatformFileToPlatformFileForTransit(
    Dispatcher* dispatcher,
    int32_t* error,
    base::PlatformFile file) {
  if (*error != PP_OK)
    return IPC::InvalidPlatformFileForTransit();
  IPC::PlatformFileForTransit out_handle =
      dispatcher->ShareHandleWithRemote(file, true);
  if (out_handle == IPC::InvalidPlatformFileForTransit())
    *error = PP_ERROR_NOACCESS;
  return out_handle;
}

void FreeDirContents(PP_Instance /* instance */,
                     PP_DirContents_Dev* contents) {
  for (int32_t i = 0; i < contents->count; ++i)
    delete[] contents->entries[i].name;
  delete[] contents->entries;
  delete contents;
}

}  // namespace

// ModuleLocalThreadAdapter ----------------------------------------------------

class ModuleLocalThreadAdapter
    : public base::RefCountedThreadSafe<ModuleLocalThreadAdapter> {
  class Filter;
 public:
  ModuleLocalThreadAdapter();

  void AddInstanceRouting(PP_Instance instance, Dispatcher* dispatcher);
  void ClearInstanceRouting(PP_Instance instance);
  void ClearFilter(Dispatcher* dispatcher, Filter* filter);

  bool OnModuleLocalMessageReceived(const IPC::Message& msg);

  // Called on the I/O thread when the channel is being destroyed and the
  // given message will never be issued a reply.
  void OnModuleLocalMessageFailed(int message_id);

  bool Send(PP_Instance instance, IPC::Message* msg);

 private:
  class Filter : public IPC::ChannelProxy::MessageFilter {
   public:
    explicit Filter(Dispatcher* dispatcher);
    ~Filter();

    void Send(IPC::Message* msg);

    virtual void OnFilterAdded(IPC::Channel* channel);
    virtual void OnFilterRemoved();
    virtual bool OnMessageReceived(const IPC::Message& message);

   private:
    // DO NOT DEREFERENCE! This is used only for tracking.
    Dispatcher* dispatcher_;

    IPC::Channel* channel_;

    // Holds the IPC messages that were sent before the channel was connected.
    // These will be sent ASAP.
    std::vector<IPC::Message*> pre_connect_pending_messages_;

    // Holds the IDs of the sync messages we're currently waiting on for this
    // channel. This tracking allows us to cancel those requests if the
    // remote process crashes and we're cleaning up this filter (without just
    // deadlocking the waiting thread(s).
    std::set<int> pending_requests_for_filter_;
  };

  void SendFromIOThread(Dispatcher* dispatcher, IPC::Message* msg);

  // Internal version of OnModuleLocalMessageFailed which assumes the lock
  // is already held.
  void OnModuleLocalMessageFailedLocked(int message_id);

  base::Lock lock_;

  scoped_refptr<base::MessageLoopProxy> main_thread_;

  // Will be NULL before an instance routing is added.
  scoped_refptr<base::MessageLoopProxy> io_thread_;

  typedef std::map<PP_Instance, Dispatcher*> InstanceToDispatcher;
  InstanceToDispatcher instance_to_dispatcher_;

  // The filters are owned by the channel.
  typedef std::map<Dispatcher*, Filter*> DispatcherToFilter;
  DispatcherToFilter dispatcher_to_filter_;

  // Tracks all messages with currently waiting threads. This does not own
  // the pointer, the pointer lifetime is managed by Send().
  typedef std::map<int, IPC::PendingSyncMsg*> SyncRequestMap;
  SyncRequestMap pending_sync_requests_;
};

ModuleLocalThreadAdapter* g_module_local_thread_adapter = NULL;

ModuleLocalThreadAdapter::Filter::Filter(Dispatcher* dispatcher)
    : dispatcher_(dispatcher), channel_(NULL) {
}

ModuleLocalThreadAdapter::Filter::~Filter() {
}

void ModuleLocalThreadAdapter::Filter::Send(IPC::Message* msg) {
  if (channel_) {
    int message_id = IPC::SyncMessage::GetMessageId(*msg);
    if (channel_->Send(msg))
      pending_requests_for_filter_.insert(message_id);
    else  // Message lost, notify adapter so it can unblock.
      g_module_local_thread_adapter->OnModuleLocalMessageFailed(message_id);
  } else {
    // No channel, save this message for when it's connected.
    pre_connect_pending_messages_.push_back(msg);
  }
}

void ModuleLocalThreadAdapter::Filter::OnFilterAdded(IPC::Channel* channel) {
  DCHECK(!channel_);
  channel_ = channel;

  // Now that we have a channel, process all pending messages.
  for (size_t i = 0; i < pre_connect_pending_messages_.size(); i++)
    Send(pre_connect_pending_messages_[i]);
  pre_connect_pending_messages_.clear();
}

void ModuleLocalThreadAdapter::Filter::OnFilterRemoved() {
  DCHECK(channel_);
  channel_ = NULL;
  g_module_local_thread_adapter->ClearFilter(dispatcher_, this);

  for (std::set<int>::iterator i = pending_requests_for_filter_.begin();
       i != pending_requests_for_filter_.end(); ++i) {
    g_module_local_thread_adapter->OnModuleLocalMessageFailed(*i);
  }
}

bool ModuleLocalThreadAdapter::Filter::OnMessageReceived(
    const IPC::Message& message) {
  if (!message.is_reply() ||
      message.routing_id() != INTERFACE_ID_PPB_FLASH_FILE_MODULELOCAL)
    return false;

  if (g_module_local_thread_adapter->OnModuleLocalMessageReceived(message)) {
    // The message was consumed, this means we can remove the message ID from
    // the list of messages this channel is waiting on.
    pending_requests_for_filter_.erase(IPC::SyncMessage::GetMessageId(message));
    return true;
  }
  return false;
}

ModuleLocalThreadAdapter::ModuleLocalThreadAdapter()
    : main_thread_(base::MessageLoopProxy::current()) {
}

void ModuleLocalThreadAdapter::AddInstanceRouting(PP_Instance instance,
                                                  Dispatcher* dispatcher) {
  base::AutoLock lock(lock_);

  // Now that we've had contact with a dispatcher, we can set up the IO thread.
  DCHECK(main_thread_->BelongsToCurrentThread());
  if (!io_thread_.get())
    io_thread_ = dispatcher->GetIPCMessageLoop();

  // Set up the instance -> dispatcher routing.
  DCHECK(instance_to_dispatcher_.find(instance) ==
         instance_to_dispatcher_.end());
  instance_to_dispatcher_[instance] = dispatcher;

  DispatcherToFilter::iterator found_filter =
      dispatcher_to_filter_.find(dispatcher);
  if (found_filter == dispatcher_to_filter_.end()) {
    // Need to set up a filter for this dispatcher to intercept the messages.
    Filter* filter = new Filter(dispatcher);
    dispatcher_to_filter_[dispatcher] = filter;
    dispatcher->AddIOThreadMessageFilter(filter);
  }
}

void ModuleLocalThreadAdapter::ClearInstanceRouting(PP_Instance instance) {
  // The dispatcher->filter mapping is cleaned up by ClearFilter which is
  // initiated by the channel.
  instance_to_dispatcher_.erase(instance);
}

void ModuleLocalThreadAdapter::ClearFilter(Dispatcher* dispatcher,
                                           Filter* filter) {
  // DANGER! Don't dereference the dispatcher, it's just used to identify
  // which filter to remove. The dispatcher may not even exist any more.
  //
  // Since the dispatcher may be gone, there's a potential for ambiguity if
  // another one is created on the main thread before this code runs on the
  // I/O thread. So we check that the filter matches to avoid this rare case.
  base::AutoLock lock(lock_);
  if (dispatcher_to_filter_[dispatcher] == filter)
    dispatcher_to_filter_.erase(dispatcher);
}

bool ModuleLocalThreadAdapter::OnModuleLocalMessageReceived(
    const IPC::Message& msg) {
  base::AutoLock lock(lock_);

  int message_id = IPC::SyncMessage::GetMessageId(msg);
  SyncRequestMap::iterator found = pending_sync_requests_.find(message_id);
  if (found == pending_sync_requests_.end()) {
    // Not waiting for this event. This will happen for sync messages to the
    // main thread which use the "regular" sync channel code path.
    return false;
  }

  IPC::PendingSyncMsg& info = *found->second;

  if (!msg.is_reply_error())
    info.deserializer->SerializeOutputParameters(msg);
  info.done_event->Signal();
  return true;
}

void ModuleLocalThreadAdapter::OnModuleLocalMessageFailed(int message_id) {
  base::AutoLock lock(lock_);
  OnModuleLocalMessageFailedLocked(message_id);
}

bool ModuleLocalThreadAdapter::Send(PP_Instance instance, IPC::Message* msg) {
  // Compute the dispatcher corresponding to this message.
  Dispatcher* dispatcher = NULL;
  {
    base::AutoLock lock(lock_);
    InstanceToDispatcher::iterator found =
        instance_to_dispatcher_.find(instance);
    if (found == instance_to_dispatcher_.end()) {
      NOTREACHED();
      delete msg;
      return false;
    }
    dispatcher = found->second;
  }

  if (main_thread_->BelongsToCurrentThread()) {
    // Easy case: We're on the same thread as the dispatcher, so we don't need
    // a lock to access it, and we can just use the normal sync channel stuff
    // to handle the message. Actually, we MUST use the normal sync channel
    // stuff since there may be incoming sync messages that need processing.
    // The code below doesn't handle any nested message loops.
    return dispatcher->Send(msg);
  }

  // Background thread case
  // ----------------------
  //  1. Generate tracking info, stick in pending_sync_messages_map.
  //  2. Kick off the request. This is done on the I/O thread.
  //  3. Filter on the I/O thread notices reply, writes the reply data and
  //     signals the event. We block on the event while this is happening.
  //  4. Remove tracking info.

  // Generate the tracking info. and copied
  IPC::SyncMessage* sync_msg = static_cast<IPC::SyncMessage*>(msg);
  int message_id = IPC::SyncMessage::GetMessageId(*sync_msg);
  base::WaitableEvent event(true, false);
  scoped_ptr<IPC::MessageReplyDeserializer> deserializer(
      sync_msg->GetReplyDeserializer());  // We own this pointer once retrieved.
  IPC::PendingSyncMsg info(message_id, deserializer.get(), &event);

  // Add the tracking information to our map.
  {
    base::AutoLock lock(lock_);
    pending_sync_requests_[message_id] = &info;
  }

  // This is a bit dangerous. We use the dispatcher pointer as the routing
  // ID for this message. While we don't dereference it, there is an
  // exceedingly remote possibility that while this is going to the background
  // thread the connection will be shut down and a new one will be created with
  // a dispatcher at the same address. It could potentially get sent to a
  // random place, but it should actually still work (since the Flash file
  // operations are global).
  io_thread_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &ModuleLocalThreadAdapter::SendFromIOThread,
      dispatcher, msg));

  // Now we block the current thread waiting for the reply.
  event.Wait();

  {
    // Clear our tracking info for this message now that we're done.
    base::AutoLock lock(lock_);
    DCHECK(pending_sync_requests_.find(message_id) !=
           pending_sync_requests_.end());
    pending_sync_requests_.erase(message_id);
  }

  return true;
}

void ModuleLocalThreadAdapter::SendFromIOThread(Dispatcher* dispatcher,
                                                IPC::Message* msg) {
  // DO NOT DEREFERENCE DISPATCHER. Used as a lookup only.
  base::AutoLock lock(lock_);
  DispatcherToFilter::iterator found = dispatcher_to_filter_.find(dispatcher);

  // The dispatcher could have been destroyed by the time we got here since
  // we're on another thread. Need to unblock the caller.
  if (found == dispatcher_to_filter_.end()) {
    OnModuleLocalMessageFailedLocked(IPC::SyncMessage::GetMessageId(*msg));
    delete msg;
    return;
  }

  // Takes ownership of pointer.
  found->second->Send(msg);
}

void ModuleLocalThreadAdapter::OnModuleLocalMessageFailedLocked(
    int message_id) {
  lock_.AssertAcquired();

  // Unblock the thread waiting for the message that will never come.
  SyncRequestMap::iterator found = pending_sync_requests_.find(message_id);
  if (found == pending_sync_requests_.end()) {
    NOTREACHED();
    return;
  }
  found->second->done_event->Signal();
}

// PPB_Flash_File_ModuleLocal --------------------------------------------------

namespace {

bool CreateThreadAdapterForInstance(PP_Instance instance) {
  if (!g_module_local_thread_adapter) {
    g_module_local_thread_adapter = new ModuleLocalThreadAdapter();
    g_module_local_thread_adapter->AddRef();  // Leaked, this object is global.
  }

  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher) {
    NOTREACHED();
    return false;
  }
  g_module_local_thread_adapter->AddInstanceRouting(instance, dispatcher);
  return true;
}

void ClearThreadAdapterForInstance(PP_Instance instance) {
  if (g_module_local_thread_adapter)
    g_module_local_thread_adapter->ClearInstanceRouting(instance);
}

int32_t OpenModuleLocalFile(PP_Instance instance,
                            const char* path,
                            int32_t mode,
                            PP_FileHandle* file) {
  if (!g_module_local_thread_adapter)
    return PP_ERROR_FAILED;

  int32_t result = PP_ERROR_FAILED;
  IPC::PlatformFileForTransit transit;
  g_module_local_thread_adapter->Send(instance,
      new PpapiHostMsg_PPBFlashFile_ModuleLocal_OpenFile(
          INTERFACE_ID_PPB_FLASH_FILE_MODULELOCAL,
          instance, path, mode, &transit, &result));
  *file = IPC::PlatformFileForTransitToPlatformFile(transit);
  return result;
}

int32_t RenameModuleLocalFile(PP_Instance instance,
                              const char* from_path,
                              const char* to_path) {
  if (!g_module_local_thread_adapter)
    return PP_ERROR_FAILED;

  int32_t result = PP_ERROR_FAILED;
  g_module_local_thread_adapter->Send(instance,
      new PpapiHostMsg_PPBFlashFile_ModuleLocal_RenameFile(
          INTERFACE_ID_PPB_FLASH_FILE_MODULELOCAL,
          instance, from_path, to_path, &result));
  return result;
}

int32_t DeleteModuleLocalFileOrDir(PP_Instance instance,
                                   const char* path,
                                   PP_Bool recursive) {
  if (!g_module_local_thread_adapter)
    return PP_ERROR_FAILED;

  int32_t result = PP_ERROR_FAILED;
  g_module_local_thread_adapter->Send(instance,
      new PpapiHostMsg_PPBFlashFile_ModuleLocal_DeleteFileOrDir(
          INTERFACE_ID_PPB_FLASH_FILE_MODULELOCAL,
          instance, path, recursive, &result));
  return result;
}

int32_t CreateModuleLocalDir(PP_Instance instance, const char* path) {
  if (!g_module_local_thread_adapter)
    return PP_ERROR_FAILED;

  int32_t result = PP_ERROR_FAILED;
  g_module_local_thread_adapter->Send(instance,
      new PpapiHostMsg_PPBFlashFile_ModuleLocal_CreateDir(
          INTERFACE_ID_PPB_FLASH_FILE_MODULELOCAL, instance, path, &result));
  return result;
}

int32_t QueryModuleLocalFile(PP_Instance instance,
                             const char* path,
                             PP_FileInfo* info) {
  if (!g_module_local_thread_adapter)
    return PP_ERROR_FAILED;

  int32_t result = PP_ERROR_FAILED;
  g_module_local_thread_adapter->Send(instance,
      new PpapiHostMsg_PPBFlashFile_ModuleLocal_QueryFile(
          INTERFACE_ID_PPB_FLASH_FILE_MODULELOCAL, instance, path,
          info, &result));
  return result;
}

int32_t GetModuleLocalDirContents(PP_Instance instance,
                                  const char* path,
                                  PP_DirContents_Dev** contents) {
  if (!g_module_local_thread_adapter)
    return PP_ERROR_FAILED;

  int32_t result = PP_ERROR_FAILED;
  std::vector<SerializedDirEntry> entries;
  g_module_local_thread_adapter->Send(instance,
      new PpapiHostMsg_PPBFlashFile_ModuleLocal_GetDirContents(
          INTERFACE_ID_PPB_FLASH_FILE_MODULELOCAL,
          instance, path, &entries, &result));

  if (result != PP_OK)
    return result;

  // Copy the serialized dir entries to the output struct.
  *contents = new PP_DirContents_Dev;
  (*contents)->count = static_cast<int32_t>(entries.size());
  (*contents)->entries = new PP_DirEntry_Dev[entries.size()];
  for (size_t i = 0; i < entries.size(); i++) {
    const SerializedDirEntry& source = entries[i];
    PP_DirEntry_Dev* dest = &(*contents)->entries[i];

    char* name_copy = new char[source.name.size() + 1];
    memcpy(name_copy, source.name.c_str(), source.name.size() + 1);
    dest->name = name_copy;
    dest->is_dir = PP_FromBool(source.is_dir);
  }

  return result;
}

const PPB_Flash_File_ModuleLocal flash_file_modulelocal_interface = {
  &CreateThreadAdapterForInstance,
  &ClearThreadAdapterForInstance,
  &OpenModuleLocalFile,
  &RenameModuleLocalFile,
  &DeleteModuleLocalFileOrDir,
  &CreateModuleLocalDir,
  &QueryModuleLocalFile,
  &GetModuleLocalDirContents,
  &FreeDirContents,
};

InterfaceProxy* CreateFlashFileModuleLocalProxy(Dispatcher* dispatcher) {
  return new PPB_Flash_File_ModuleLocal_Proxy(dispatcher);
}

}  // namespace

PPB_Flash_File_ModuleLocal_Proxy::PPB_Flash_File_ModuleLocal_Proxy(
    Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      ppb_flash_file_module_local_impl_(NULL) {
  if (!dispatcher->IsPlugin()) {
    ppb_flash_file_module_local_impl_ =
        static_cast<const PPB_Flash_File_ModuleLocal*>(
            dispatcher->local_get_interface()(
                PPB_FLASH_FILE_MODULELOCAL_INTERFACE));
  }
}

PPB_Flash_File_ModuleLocal_Proxy::~PPB_Flash_File_ModuleLocal_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_Flash_File_ModuleLocal_Proxy::GetInfo() {
  static const Info info = {
    &flash_file_modulelocal_interface,
    PPB_FLASH_FILE_MODULELOCAL_INTERFACE,
    INTERFACE_ID_PPB_FLASH_FILE_MODULELOCAL,
    true,
    &CreateFlashFileModuleLocalProxy,
  };
  return &info;
}

bool PPB_Flash_File_ModuleLocal_Proxy::OnMessageReceived(
    const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Flash_File_ModuleLocal_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlashFile_ModuleLocal_OpenFile,
                        OnMsgOpenFile)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlashFile_ModuleLocal_RenameFile,
                        OnMsgRenameFile)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlashFile_ModuleLocal_DeleteFileOrDir,
                        OnMsgDeleteFileOrDir)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlashFile_ModuleLocal_CreateDir,
                        OnMsgCreateDir)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlashFile_ModuleLocal_QueryFile,
                        OnMsgQueryFile)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlashFile_ModuleLocal_GetDirContents,
                        OnMsgGetDirContents)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  // TODO(brettw) handle bad messages!
  return handled;
}

void PPB_Flash_File_ModuleLocal_Proxy::OnMsgOpenFile(
    PP_Instance instance,
    const std::string& path,
    int32_t mode,
    IPC::PlatformFileForTransit* file_handle,
    int32_t* result) {
  base::PlatformFile file;
  *result = ppb_flash_file_module_local_impl_->OpenFile(
      instance, path.c_str(), mode, &file);
  *file_handle = PlatformFileToPlatformFileForTransit(
      dispatcher(), result, file);
}

void PPB_Flash_File_ModuleLocal_Proxy::OnMsgRenameFile(
    PP_Instance instance,
    const std::string& from_path,
    const std::string& to_path,
    int32_t* result) {
  *result = ppb_flash_file_module_local_impl_->RenameFile(
      instance, from_path.c_str(), to_path.c_str());
}

void PPB_Flash_File_ModuleLocal_Proxy::OnMsgDeleteFileOrDir(
    PP_Instance instance,
    const std::string& path,
    PP_Bool recursive,
    int32_t* result) {
  *result = ppb_flash_file_module_local_impl_->DeleteFileOrDir(
      instance, path.c_str(), recursive);
}

void PPB_Flash_File_ModuleLocal_Proxy::OnMsgCreateDir(PP_Instance instance,
                                                      const std::string& path,
                                                      int32_t* result) {
  *result = ppb_flash_file_module_local_impl_->CreateDir(
      instance, path.c_str());
}

void PPB_Flash_File_ModuleLocal_Proxy::OnMsgQueryFile(PP_Instance instance,
                                                      const std::string& path,
                                                      PP_FileInfo* info,
                                                      int32_t* result) {
  *result = ppb_flash_file_module_local_impl_->QueryFile(
      instance, path.c_str(), info);
}

void PPB_Flash_File_ModuleLocal_Proxy::OnMsgGetDirContents(
    PP_Instance instance,
    const std::string& path,
    std::vector<SerializedDirEntry>* entries,
    int32_t* result) {
  PP_DirContents_Dev* contents = NULL;
  *result = ppb_flash_file_module_local_impl_->GetDirContents(
      instance, path.c_str(), &contents);
  if (*result != PP_OK)
    return;

  // Convert the list of entries to the serialized version.
  entries->resize(contents->count);
  for (int32_t i = 0; i < contents->count; i++) {
    (*entries)[i].name.assign(contents->entries[i].name);
    (*entries)[i].is_dir = PP_ToBool(contents->entries[i].is_dir);
  }
  ppb_flash_file_module_local_impl_->FreeDirContents(instance, contents);
}

// PPB_Flash_File_FileRef ------------------------------------------------------

namespace {

int32_t OpenFileRefFile(PP_Resource file_ref_id,
                        int32_t mode,
                        PP_FileHandle* file) {
  Resource* file_ref =
      PluginResourceTracker::GetInstance()->GetResource(file_ref_id);
  if (!file_ref)
    return PP_ERROR_BADRESOURCE;

  PluginDispatcher* dispatcher = PluginDispatcher::GetForResource(file_ref);
  if (!dispatcher)
    return PP_ERROR_BADARGUMENT;

  int32_t result = PP_ERROR_FAILED;
  IPC::PlatformFileForTransit transit;
  dispatcher->Send(new PpapiHostMsg_PPBFlashFile_FileRef_OpenFile(
      INTERFACE_ID_PPB_FLASH_FILE_FILEREF,
      file_ref->host_resource(), mode, &transit, &result));
  *file = IPC::PlatformFileForTransitToPlatformFile(transit);
  return result;
}

int32_t QueryFileRefFile(PP_Resource file_ref_id,
                         PP_FileInfo* info) {
  Resource* file_ref =
      PluginResourceTracker::GetInstance()->GetResource(file_ref_id);
  if (!file_ref)
    return PP_ERROR_BADRESOURCE;

  PluginDispatcher* dispatcher = PluginDispatcher::GetForResource(file_ref);
  if (!dispatcher)
    return PP_ERROR_BADARGUMENT;

  int32_t result = PP_ERROR_FAILED;
  dispatcher->Send(new PpapiHostMsg_PPBFlashFile_FileRef_QueryFile(
      INTERFACE_ID_PPB_FLASH_FILE_FILEREF,
      file_ref->host_resource(), info, &result));
  return result;
}

const PPB_Flash_File_FileRef flash_file_fileref_interface = {
  &OpenFileRefFile,
  &QueryFileRefFile,
};

InterfaceProxy* CreateFlashFileFileRefProxy(Dispatcher* dispatcher) {
  return new PPB_Flash_File_FileRef_Proxy(dispatcher);
}

}  // namespace

PPB_Flash_File_FileRef_Proxy::PPB_Flash_File_FileRef_Proxy(
    Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      ppb_flash_file_fileref_impl_(NULL) {
  if (!dispatcher->IsPlugin()) {
    ppb_flash_file_fileref_impl_ = static_cast<const PPB_Flash_File_FileRef*>(
        dispatcher->local_get_interface()(PPB_FLASH_FILE_FILEREF_INTERFACE));
  }
}

PPB_Flash_File_FileRef_Proxy::~PPB_Flash_File_FileRef_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_Flash_File_FileRef_Proxy::GetInfo() {
  static const Info info = {
    &flash_file_fileref_interface,
    PPB_FLASH_FILE_FILEREF_INTERFACE,
    INTERFACE_ID_PPB_FLASH_FILE_FILEREF,
    true,
    &CreateFlashFileFileRefProxy,
  };
  return &info;
}

bool PPB_Flash_File_FileRef_Proxy::OnMessageReceived(
    const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Flash_File_FileRef_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlashFile_FileRef_OpenFile,
                        OnMsgOpenFile)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlashFile_FileRef_QueryFile,
                        OnMsgQueryFile)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  // TODO(brettw) handle bad messages!
  return handled;
}

void PPB_Flash_File_FileRef_Proxy::OnMsgOpenFile(
    const HostResource& host_resource,
    int32_t mode,
    IPC::PlatformFileForTransit* file_handle,
    int32_t* result) {
  base::PlatformFile file;
  *result = ppb_flash_file_fileref_impl_->OpenFile(
      host_resource.host_resource(), mode, &file);
  *file_handle = PlatformFileToPlatformFileForTransit(
      dispatcher(), result, file);
}

void PPB_Flash_File_FileRef_Proxy::OnMsgQueryFile(
    const HostResource& host_resource,
    PP_FileInfo* info,
    int32_t* result) {
  *result = ppb_flash_file_fileref_impl_->QueryFile(
      host_resource.host_resource(), info);
}

}  // namespace proxy
}  // namespace ppapi
