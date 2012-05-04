// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_flash_proxy.h"

#include <map>
#include <set>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/synchronization/lock.h"
#include "base/time.h"
#include "ipc/ipc_channel_proxy.h"
#include "ppapi/c/dev/ppb_font_dev.h"
#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/plugin_proxy_delegate.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/proxy_module.h"
#include "ppapi/proxy/serialized_var.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/shared_impl/scoped_pp_resource.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/ppb_url_request_info_api.h"
#include "ppapi/thunk/resource_creation_api.h"

using ppapi::thunk::EnterInstanceNoLock;
using ppapi::thunk::EnterResourceNoLock;

namespace ppapi {
namespace proxy {

namespace {

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

// ModuleLocalThreadAdapter ----------------------------------------------------
// TODO(yzshen): Refactor to use IPC::SyncMessageFilter.
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
      message.routing_id() != API_ID_PPB_FLASH)
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
      base::Bind(&ModuleLocalThreadAdapter::SendFromIOThread, this,
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

}  // namespace

// -----------------------------------------------------------------------------

PPB_Flash_Proxy::PPB_Flash_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher) {
}

PPB_Flash_Proxy::~PPB_Flash_Proxy() {
}

bool PPB_Flash_Proxy::OnMessageReceived(const IPC::Message& msg) {
  // Prevent the dispatcher from going away during a call to Navigate.
  // This must happen OUTSIDE of OnMsgNavigate since the handling code use
  // the dispatcher upon return of the function (sending the reply message).
  ScopedModuleReference death_grip(dispatcher());

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Flash_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_SetInstanceAlwaysOnTop,
                        OnHostMsgSetInstanceAlwaysOnTop)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_DrawGlyphs,
                        OnHostMsgDrawGlyphs)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_GetProxyForURL,
                        OnHostMsgGetProxyForURL)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_Navigate, OnHostMsgNavigate)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_RunMessageLoop,
                        OnHostMsgRunMessageLoop)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_QuitMessageLoop,
                        OnHostMsgQuitMessageLoop)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_GetLocalTimeZoneOffset,
                        OnHostMsgGetLocalTimeZoneOffset)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_IsRectTopmost,
                        OnHostMsgIsRectTopmost)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_FlashSetFullscreen,
                        OnHostMsgFlashSetFullscreen)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_FlashGetScreenSize,
                        OnHostMsgFlashGetScreenSize)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_IsClipboardFormatAvailable,
                        OnHostMsgIsClipboardFormatAvailable)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_ReadClipboardData,
                        OnHostMsgReadClipboardData)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_WriteClipboardData,
                        OnHostMsgWriteClipboardData)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_OpenFile,
                        OnHostMsgOpenFile)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_RenameFile,
                        OnHostMsgRenameFile)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_DeleteFileOrDir,
                        OnHostMsgDeleteFileOrDir)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_CreateDir,
                        OnHostMsgCreateDir)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_QueryFile,
                        OnHostMsgQueryFile)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_GetDirContents,
                        OnHostMsgGetDirContents)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_OpenFileRef,
                        OnHostMsgOpenFileRef)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_QueryFileRef,
                        OnHostMsgQueryFileRef)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_GetDeviceID,
                        OnHostMsgGetDeviceID)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  // TODO(brettw) handle bad messages!
  return handled;
}

void PPB_Flash_Proxy::SetInstanceAlwaysOnTop(PP_Instance instance,
                                             PP_Bool on_top) {
  dispatcher()->Send(new PpapiHostMsg_PPBFlash_SetInstanceAlwaysOnTop(
      API_ID_PPB_FLASH, instance, on_top));
}

PP_Bool PPB_Flash_Proxy::DrawGlyphs(PP_Instance instance,
                                    PP_Resource pp_image_data,
                                    const PP_FontDescription_Dev* font_desc,
                                    uint32_t color,
                                    const PP_Point* position,
                                    const PP_Rect* clip,
                                    const float transformation[3][3],
                                    PP_Bool allow_subpixel_aa,
                                    uint32_t glyph_count,
                                    const uint16_t glyph_indices[],
                                    const PP_Point glyph_advances[]) {
  Resource* image_data =
      PpapiGlobals::Get()->GetResourceTracker()->GetResource(pp_image_data);
  if (!image_data)
    return PP_FALSE;
  // The instance parameter isn't strictly necessary but we check that it
  // matches anyway.
  if (image_data->pp_instance() != instance)
    return PP_FALSE;

  PPBFlash_DrawGlyphs_Params params;
  params.image_data = image_data->host_resource();
  params.font_desc.SetFromPPFontDescription(dispatcher(), *font_desc, true);
  params.color = color;
  params.position = *position;
  params.clip = *clip;
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++)
      params.transformation[i][j] = transformation[i][j];
  }
  params.allow_subpixel_aa = allow_subpixel_aa;

  params.glyph_indices.insert(params.glyph_indices.begin(),
                              &glyph_indices[0],
                              &glyph_indices[glyph_count]);
  params.glyph_advances.insert(params.glyph_advances.begin(),
                               &glyph_advances[0],
                               &glyph_advances[glyph_count]);

  PP_Bool result = PP_FALSE;
  dispatcher()->Send(new PpapiHostMsg_PPBFlash_DrawGlyphs(
      API_ID_PPB_FLASH, instance, params, &result));
  return result;
}

PP_Var PPB_Flash_Proxy::GetProxyForURL(PP_Instance instance, const char* url) {
  ReceiveSerializedVarReturnValue result;
  dispatcher()->Send(new PpapiHostMsg_PPBFlash_GetProxyForURL(
      API_ID_PPB_FLASH, instance, url, &result));
  return result.Return(dispatcher());
}

int32_t PPB_Flash_Proxy::Navigate(PP_Instance instance,
                                  PP_Resource request_info,
                                  const char* target,
                                  PP_Bool from_user_action) {
  thunk::EnterResourceNoLock<thunk::PPB_URLRequestInfo_API> enter(
      request_info, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;

  int32_t result = PP_ERROR_FAILED;
  dispatcher()->Send(new PpapiHostMsg_PPBFlash_Navigate(
      API_ID_PPB_FLASH,
      instance, enter.object()->GetData(), target, from_user_action,
      &result));
  return result;
}

void PPB_Flash_Proxy::RunMessageLoop(PP_Instance instance) {
  IPC::SyncMessage* msg = new PpapiHostMsg_PPBFlash_RunMessageLoop(
      API_ID_PPB_FLASH, instance);
  msg->EnableMessagePumping();
  dispatcher()->Send(msg);
}

void PPB_Flash_Proxy::QuitMessageLoop(PP_Instance instance) {
  dispatcher()->Send(new PpapiHostMsg_PPBFlash_QuitMessageLoop(
      API_ID_PPB_FLASH, instance));
}

double PPB_Flash_Proxy::GetLocalTimeZoneOffset(PP_Instance instance,
                                               PP_Time t) {
  // TODO(brettw) on Windows it should be possible to do the time calculation
  // in-process since it doesn't need to read files on disk. This will improve
  // performance.
  //
  // On Linux, it would be better to go directly to the browser process for
  // this message rather than proxy it through some instance in a renderer.
  double result = 0;
  dispatcher()->Send(new PpapiHostMsg_PPBFlash_GetLocalTimeZoneOffset(
      API_ID_PPB_FLASH, instance, t, &result));
  return result;
}

PP_Bool PPB_Flash_Proxy::IsRectTopmost(PP_Instance instance,
                                       const PP_Rect* rect) {
  PP_Bool result = PP_FALSE;
  dispatcher()->Send(new PpapiHostMsg_PPBFlash_IsRectTopmost(
      API_ID_PPB_FLASH, instance, *rect, &result));
  return result;
}

int32_t PPB_Flash_Proxy::InvokePrinting(PP_Instance instance) {
  return PP_ERROR_NOTSUPPORTED;
}

void PPB_Flash_Proxy::UpdateActivity(PP_Instance instance) {
  PluginGlobals::Get()->plugin_proxy_delegate()->SendToBrowser(
      new PpapiHostMsg_PPBFlash_UpdateActivity(API_ID_PPB_FLASH));
}

PP_Var PPB_Flash_Proxy::GetDeviceID(PP_Instance instance) {
  ReceiveSerializedVarReturnValue result;
  dispatcher()->Send(new PpapiHostMsg_PPBFlash_GetDeviceID(
      API_ID_PPB_FLASH, instance, &result));
  return result.Return(dispatcher());
}

int32_t PPB_Flash_Proxy::GetSettingInt(PP_Instance instance,
                                       PP_FlashSetting setting) {
  switch (setting) {
    case PP_FLASHSETTING_3DENABLED:
      return static_cast<PluginDispatcher*>(dispatcher())->preferences().
          is_3d_supported;
    case PP_FLASHSETTING_INCOGNITO:
      return static_cast<PluginDispatcher*>(dispatcher())->incognito();
    default:
      return -1;
  }
}

PP_Bool PPB_Flash_Proxy::IsClipboardFormatAvailable(
    PP_Instance instance,
    PP_Flash_Clipboard_Type clipboard_type,
    PP_Flash_Clipboard_Format format) {
  if (!IsValidClipboardType(clipboard_type) || !IsValidClipboardFormat(format))
    return PP_FALSE;

  bool result = false;
  dispatcher()->Send(new PpapiHostMsg_PPBFlash_IsClipboardFormatAvailable(
      API_ID_PPB_FLASH,
      instance,
      static_cast<int>(clipboard_type),
      static_cast<int>(format),
      &result));
  return PP_FromBool(result);
}

PP_Var PPB_Flash_Proxy::ReadClipboardData(
    PP_Instance instance,
    PP_Flash_Clipboard_Type clipboard_type,
    PP_Flash_Clipboard_Format format) {
  if (!IsValidClipboardType(clipboard_type) || !IsValidClipboardFormat(format))
    return PP_MakeUndefined();

  ReceiveSerializedVarReturnValue result;
  dispatcher()->Send(new PpapiHostMsg_PPBFlash_ReadClipboardData(
      API_ID_PPB_FLASH, instance,
      static_cast<int>(clipboard_type), static_cast<int>(format), &result));
  return result.Return(dispatcher());
}

int32_t PPB_Flash_Proxy::WriteClipboardData(
    PP_Instance instance,
    PP_Flash_Clipboard_Type clipboard_type,
    uint32_t data_item_count,
    const PP_Flash_Clipboard_Format formats[],
    const PP_Var data_items[]) {
  if (!IsValidClipboardType(clipboard_type))
    return PP_ERROR_BADARGUMENT;

  std::vector<SerializedVar> data_items_vector;
  SerializedVarSendInput::ConvertVector(
      dispatcher(),
      data_items,
      data_item_count,
      &data_items_vector);
  for (size_t i = 0; i < data_item_count; ++i) {
    if (!IsValidClipboardFormat(formats[i]))
      return PP_ERROR_BADARGUMENT;
  }

  std::vector<int> formats_vector(formats, formats + data_item_count);
  dispatcher()->Send(new PpapiHostMsg_PPBFlash_WriteClipboardData(
      API_ID_PPB_FLASH,
      instance,
      static_cast<int>(clipboard_type),
      formats_vector,
      data_items_vector));
  // Assume success, since it allows us to avoid a sync IPC.
  return PP_OK;
}

bool PPB_Flash_Proxy::CreateThreadAdapterForInstance(PP_Instance instance) {
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

void PPB_Flash_Proxy::ClearThreadAdapterForInstance(PP_Instance instance) {
  if (g_module_local_thread_adapter)
    g_module_local_thread_adapter->ClearInstanceRouting(instance);
}

int32_t PPB_Flash_Proxy::OpenFile(PP_Instance instance,
                                  const char* path,
                                  int32_t mode,
                                  PP_FileHandle* file) {
  if (!g_module_local_thread_adapter)
    return PP_ERROR_FAILED;

  int32_t result = PP_ERROR_FAILED;
  IPC::PlatformFileForTransit transit;
  g_module_local_thread_adapter->Send(instance,
      new PpapiHostMsg_PPBFlash_OpenFile(
          API_ID_PPB_FLASH, instance, path, mode, &transit, &result));
  *file = IPC::PlatformFileForTransitToPlatformFile(transit);
  return result;
}

int32_t PPB_Flash_Proxy::RenameFile(PP_Instance instance,
                                    const char* path_from,
                                    const char* path_to) {
  if (!g_module_local_thread_adapter)
    return PP_ERROR_FAILED;

  int32_t result = PP_ERROR_FAILED;
  g_module_local_thread_adapter->Send(instance,
      new PpapiHostMsg_PPBFlash_RenameFile(
          API_ID_PPB_FLASH, instance, path_from, path_to, &result));
  return result;
}

int32_t PPB_Flash_Proxy::DeleteFileOrDir(PP_Instance instance,
                                         const char* path,
                                         PP_Bool recursive) {
  if (!g_module_local_thread_adapter)
    return PP_ERROR_FAILED;

  int32_t result = PP_ERROR_FAILED;
  g_module_local_thread_adapter->Send(instance,
      new PpapiHostMsg_PPBFlash_DeleteFileOrDir(
          API_ID_PPB_FLASH, instance, path, recursive, &result));
  return result;
}

int32_t PPB_Flash_Proxy::CreateDir(PP_Instance instance, const char* path) {
  if (!g_module_local_thread_adapter)
    return PP_ERROR_FAILED;

  int32_t result = PP_ERROR_FAILED;
  g_module_local_thread_adapter->Send(instance,
      new PpapiHostMsg_PPBFlash_CreateDir(
          API_ID_PPB_FLASH, instance, path, &result));
  return result;
}

int32_t PPB_Flash_Proxy::QueryFile(PP_Instance instance,
                                   const char* path,
                                   PP_FileInfo* info) {
  if (!g_module_local_thread_adapter)
    return PP_ERROR_FAILED;

  int32_t result = PP_ERROR_FAILED;
  g_module_local_thread_adapter->Send(instance,
      new PpapiHostMsg_PPBFlash_QueryFile(
          API_ID_PPB_FLASH, instance, path, info, &result));
  return result;
}

int32_t PPB_Flash_Proxy::GetDirContents(PP_Instance instance,
                                        const char* path,
                                        PP_DirContents_Dev** contents) {
  if (!g_module_local_thread_adapter)
    return PP_ERROR_FAILED;

  int32_t result = PP_ERROR_FAILED;
  std::vector<SerializedDirEntry> entries;
  g_module_local_thread_adapter->Send(instance,
      new PpapiHostMsg_PPBFlash_GetDirContents(
          API_ID_PPB_FLASH, instance, path, &entries, &result));

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

int32_t PPB_Flash_Proxy::OpenFileRef(PP_Instance instance,
                                     PP_Resource file_ref_id,
                                     int32_t mode,
                                     PP_FileHandle* file) {
  EnterResourceNoLock<PPB_FileRef> enter(file_ref_id, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;

  int32_t result = PP_ERROR_FAILED;
  IPC::PlatformFileForTransit transit;
  dispatcher()->Send(new PpapiHostMsg_PPBFlash_OpenFileRef(
      API_ID_PPB_FLASH, instance, enter.resource()->host_resource(), mode,
      &transit, &result));
  *file = IPC::PlatformFileForTransitToPlatformFile(transit);
  return result;
}

int32_t PPB_Flash_Proxy::QueryFileRef(PP_Instance instance,
                                      PP_Resource file_ref_id,
                                      PP_FileInfo* info) {
  EnterResourceNoLock<PPB_FileRef> enter(file_ref_id, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;

  int32_t result = PP_ERROR_FAILED;
  dispatcher()->Send(new PpapiHostMsg_PPBFlash_QueryFileRef(
      API_ID_PPB_FLASH, instance, enter.resource()->host_resource(), info,
      &result));
  return result;
}

PP_Bool PPB_Flash_Proxy::FlashIsFullscreen(PP_Instance instance) {
  InstanceData* data = static_cast<PluginDispatcher*>(dispatcher())->
      GetInstanceData(instance);
  if (!data)
    return PP_FALSE;
  return data->flash_fullscreen;
}

PP_Bool PPB_Flash_Proxy::FlashSetFullscreen(PP_Instance instance,
                                            PP_Bool fullscreen) {
  PP_Bool result = PP_FALSE;
  dispatcher()->Send(new PpapiHostMsg_PPBFlash_FlashSetFullscreen(
      API_ID_PPB_FLASH, instance, fullscreen, &result));
  return result;
}

PP_Bool PPB_Flash_Proxy::FlashGetScreenSize(PP_Instance instance,
                                            PP_Size* size) {
  PP_Bool result = PP_FALSE;
  dispatcher()->Send(new PpapiHostMsg_PPBFlash_FlashGetScreenSize(
      API_ID_PPB_FLASH, instance, &result, size));
  return result;
}

void PPB_Flash_Proxy::OnHostMsgSetInstanceAlwaysOnTop(PP_Instance instance,
                                                      PP_Bool on_top) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded())
    enter.functions()->GetFlashAPI()->SetInstanceAlwaysOnTop(instance, on_top);
}

void PPB_Flash_Proxy::OnHostMsgDrawGlyphs(
    PP_Instance instance,
    const PPBFlash_DrawGlyphs_Params& params,
    PP_Bool* result) {
  *result = PP_FALSE;
  EnterInstanceNoLock enter(instance);
  if (enter.failed())
    return;

  PP_FontDescription_Dev font_desc;
  params.font_desc.SetToPPFontDescription(dispatcher(), &font_desc, false);

  if (params.glyph_indices.size() != params.glyph_advances.size() ||
      params.glyph_indices.empty())
    return;

  *result = enter.functions()->GetFlashAPI()->DrawGlyphs(
      0,  // Unused instance param.
      params.image_data.host_resource(), &font_desc,
      params.color, &params.position, &params.clip,
      const_cast<float(*)[3]>(params.transformation),
      params.allow_subpixel_aa,
      static_cast<uint32_t>(params.glyph_indices.size()),
      const_cast<uint16_t*>(&params.glyph_indices[0]),
      const_cast<PP_Point*>(&params.glyph_advances[0]));
}

void PPB_Flash_Proxy::OnHostMsgGetProxyForURL(PP_Instance instance,
                                              const std::string& url,
                                              SerializedVarReturnValue result) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded()) {
    result.Return(dispatcher(),
                  enter.functions()->GetFlashAPI()->GetProxyForURL(
                      instance, url.c_str()));
  } else {
    result.Return(dispatcher(), PP_MakeUndefined());
  }
}

void PPB_Flash_Proxy::OnHostMsgNavigate(PP_Instance instance,
                                        const PPB_URLRequestInfo_Data& data,
                                        const std::string& target,
                                        PP_Bool from_user_action,
                                        int32_t* result) {
  EnterInstanceNoLock enter_instance(instance);
  if (enter_instance.failed()) {
    *result = PP_ERROR_BADARGUMENT;
    return;
  }
  DCHECK(!dispatcher()->IsPlugin());

  // Validate the PP_Instance since we'll be constructing resources on its
  // behalf.
  HostDispatcher* host_dispatcher = static_cast<HostDispatcher*>(dispatcher());
  if (HostDispatcher::GetForInstance(instance) != host_dispatcher) {
    NOTREACHED();
    *result = PP_ERROR_BADARGUMENT;
    return;
  }

  // We need to allow re-entrancy here, because this may call into Javascript
  // (e.g. with a "javascript:" URL), or do things like navigate away from the
  // page, either one of which will need to re-enter into the plugin.
  // It is safe, because it is essentially equivalent to NPN_GetURL, where Flash
  // would expect re-entrancy. When running in-process, it does re-enter here.
  host_dispatcher->set_allow_plugin_reentrancy();

  // Make a temporary request resource.
  thunk::EnterResourceCreation enter(instance);
  if (enter.failed()) {
    *result = PP_ERROR_FAILED;
    return;
  }
  ScopedPPResource request_resource(
      ScopedPPResource::PassRef(),
      enter.functions()->CreateURLRequestInfo(instance, data));

  *result = enter_instance.functions()->GetFlashAPI()->Navigate(
      instance, request_resource, target.c_str(), from_user_action);
}

void PPB_Flash_Proxy::OnHostMsgRunMessageLoop(PP_Instance instance) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded())
    enter.functions()->GetFlashAPI()->RunMessageLoop(instance);
}

void PPB_Flash_Proxy::OnHostMsgQuitMessageLoop(PP_Instance instance) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded())
    enter.functions()->GetFlashAPI()->QuitMessageLoop(instance);
}

void PPB_Flash_Proxy::OnHostMsgGetLocalTimeZoneOffset(PP_Instance instance,
                                                  PP_Time t,
                                                  double* result) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded()) {
    *result = enter.functions()->GetFlashAPI()->GetLocalTimeZoneOffset(
        instance, t);
  } else {
    *result = 0.0;
  }
}

void PPB_Flash_Proxy::OnHostMsgIsRectTopmost(PP_Instance instance,
                                             PP_Rect rect,
                                             PP_Bool* result) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded())
    *result = enter.functions()->GetFlashAPI()->IsRectTopmost(instance, &rect);
  else
    *result = PP_FALSE;
}

void PPB_Flash_Proxy::OnHostMsgFlashSetFullscreen(PP_Instance instance,
                                                  PP_Bool fullscreen,
                                                  PP_Bool* result) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded()) {
    *result = enter.functions()->GetFlashAPI()->FlashSetFullscreen(
        instance, fullscreen);
  } else {
    *result = PP_FALSE;
  }
}

void PPB_Flash_Proxy::OnHostMsgFlashGetScreenSize(PP_Instance instance,
                                                  PP_Bool* result,
                                                  PP_Size* size) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded()) {
    *result = enter.functions()->GetFlashAPI()->FlashGetScreenSize(
        instance, size);
  } else {
    size->width = 0;
    size->height = 0;
  }
}

void PPB_Flash_Proxy::OnHostMsgIsClipboardFormatAvailable(
    PP_Instance instance,
    int clipboard_type,
    int format,
    bool* result) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded()) {
    *result = PP_ToBool(
        enter.functions()->GetFlashAPI()->IsClipboardFormatAvailable(
            instance,
            static_cast<PP_Flash_Clipboard_Type>(clipboard_type),
            static_cast<PP_Flash_Clipboard_Format>(format)));
  } else {
    *result = false;
  }
}

void PPB_Flash_Proxy::OnHostMsgReadClipboardData(
    PP_Instance instance,
    int clipboard_type,
    int format,
    SerializedVarReturnValue result) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded()) {
    result.Return(dispatcher(),
                  enter.functions()->GetFlashAPI()->ReadClipboardData(
                      instance,
                      static_cast<PP_Flash_Clipboard_Type>(clipboard_type),
                      static_cast<PP_Flash_Clipboard_Format>(format)));
  }
}

void PPB_Flash_Proxy::OnHostMsgWriteClipboardData(
    PP_Instance instance,
    int clipboard_type,
    const std::vector<int>& formats,
    SerializedVarVectorReceiveInput data_items) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded()) {
    uint32_t data_item_count;
    PP_Var* data_items_array = data_items.Get(dispatcher(), &data_item_count);
    CHECK(data_item_count == formats.size());

    scoped_array<PP_Flash_Clipboard_Format> formats_array(
        new PP_Flash_Clipboard_Format[formats.size()]);
    for (uint32_t i = 0; i < formats.size(); ++i)
      formats_array[i] = static_cast<PP_Flash_Clipboard_Format>(formats[i]);

    int32_t result = enter.functions()->GetFlashAPI()->WriteClipboardData(
        instance,
        static_cast<PP_Flash_Clipboard_Type>(clipboard_type),
        data_item_count,
        formats_array.get(),
        data_items_array);
    DLOG_IF(WARNING, result != PP_OK)
        << "Write to clipboard failed unexpectedly.";
    (void)result;  // Prevent warning in release mode.
  }
}

void PPB_Flash_Proxy::OnHostMsgOpenFile(
    PP_Instance instance,
    const std::string& path,
    int32_t mode,
    IPC::PlatformFileForTransit* file_handle,
    int32_t* result) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded()) {
    base::PlatformFile file;
    *result = enter.functions()->GetFlashAPI()->OpenFile(
        instance, path.c_str(), mode, &file);
    *file_handle = PlatformFileToPlatformFileForTransit(
        dispatcher(), result, file);
  } else {
    *result = PP_ERROR_BADARGUMENT;
  }
}

void PPB_Flash_Proxy::OnHostMsgRenameFile(PP_Instance instance,
                                          const std::string& from_path,
                                          const std::string& to_path,
                                          int32_t* result) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded()) {
    *result = enter.functions()->GetFlashAPI()->RenameFile(
        instance, from_path.c_str(), to_path.c_str());
  } else {
    *result = PP_ERROR_BADARGUMENT;
  }
}

void PPB_Flash_Proxy::OnHostMsgDeleteFileOrDir(PP_Instance instance,
                                               const std::string& path,
                                               PP_Bool recursive,
                                               int32_t* result) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded()) {
    *result = enter.functions()->GetFlashAPI()->DeleteFileOrDir(
        instance, path.c_str(), recursive);
  } else {
    *result = PP_ERROR_BADARGUMENT;
  }
}

void PPB_Flash_Proxy::OnHostMsgCreateDir(PP_Instance instance,
                                         const std::string& path,
                                         int32_t* result) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded()) {
    *result = enter.functions()->GetFlashAPI()->CreateDir(
        instance, path.c_str());
  } else {
    *result = PP_ERROR_BADARGUMENT;
  }
}

void PPB_Flash_Proxy::OnHostMsgQueryFile(PP_Instance instance,
                                         const std::string& path,
                                         PP_FileInfo* info,
                                         int32_t* result) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded()) {
    *result = enter.functions()->GetFlashAPI()->QueryFile(
        instance, path.c_str(), info);
  } else {
    *result = PP_ERROR_BADARGUMENT;
  }
}

void PPB_Flash_Proxy::OnHostMsgGetDirContents(
    PP_Instance instance,
    const std::string& path,
    std::vector<SerializedDirEntry>* entries,
    int32_t* result) {
  EnterInstanceNoLock enter(instance);
  if (enter.failed()) {
    *result = PP_ERROR_BADARGUMENT;
    return;
  }

  PP_DirContents_Dev* contents = NULL;
  *result = enter.functions()->GetFlashAPI()->GetDirContents(
      instance, path.c_str(), &contents);
  if (*result != PP_OK)
    return;

  // Convert the list of entries to the serialized version.
  entries->resize(contents->count);
  for (int32_t i = 0; i < contents->count; i++) {
    (*entries)[i].name.assign(contents->entries[i].name);
    (*entries)[i].is_dir = PP_ToBool(contents->entries[i].is_dir);
  }
  enter.functions()->GetFlashAPI()->FreeDirContents(instance, contents);
}

void PPB_Flash_Proxy::OnHostMsgOpenFileRef(
    PP_Instance instance,
    const HostResource& host_resource,
    int32_t mode,
    IPC::PlatformFileForTransit* file_handle,
    int32_t* result) {
  EnterInstanceNoLock enter(instance);
  if (enter.failed()) {
    *result = PP_ERROR_BADARGUMENT;
    return;
  }

  base::PlatformFile file;
  *result = enter.functions()->GetFlashAPI()->OpenFileRef(
      instance, host_resource.host_resource(), mode, &file);
  *file_handle = PlatformFileToPlatformFileForTransit(dispatcher(),
                                                      result, file);
}

void PPB_Flash_Proxy::OnHostMsgQueryFileRef(
    PP_Instance instance,
    const HostResource& host_resource,
    PP_FileInfo* info,
    int32_t* result) {
  EnterInstanceNoLock enter(instance);
  if (enter.failed()) {
    *result = PP_ERROR_BADARGUMENT;
    return;
  }
  *result = enter.functions()->GetFlashAPI()->QueryFileRef(
      instance, host_resource.host_resource(), info);
}

void PPB_Flash_Proxy::OnHostMsgGetDeviceID(PP_Instance instance,
                                           SerializedVarReturnValue id) {
  EnterInstanceNoLock enter(instance);
  if (enter.succeeded()) {
    id.Return(dispatcher(),
                  enter.functions()->GetFlashAPI()->GetDeviceID(
                      instance));
  } else {
    id.Return(dispatcher(), PP_MakeUndefined());
  }
}

}  // namespace proxy
}  // namespace ppapi
