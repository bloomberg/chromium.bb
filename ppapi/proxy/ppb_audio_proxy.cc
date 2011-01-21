// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_audio_proxy.h"

#include "base/threading/simple_thread.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/c/trusted/ppb_audio_trusted.h"
#include "ppapi/proxy/interface_id.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/audio_impl.h"

namespace pp {
namespace proxy {

class Audio : public PluginResource, public pp::shared_impl::AudioImpl {
 public:
  Audio(PP_Instance instance,
        PP_Resource config_id,
        PPB_Audio_Callback callback,
        void* user_data)
      : PluginResource(instance),
        config_(config_id) {
    SetCallback(callback, user_data);
    PluginResourceTracker::GetInstance()->AddRefResource(config_);
  }
  virtual ~Audio() {
    PluginResourceTracker::GetInstance()->ReleaseResource(config_);
  }

  // Resource overrides.
  virtual Audio* AsAudio() { return this; }

  PP_Resource config() const { return config_; }

  void StartPlayback(PP_Resource resource) {
    if (playing())
      return;
    SetStartPlaybackState();
    PluginDispatcher::GetForInstance(instance())->Send(
        new PpapiHostMsg_PPBAudio_StartOrStop(
            INTERFACE_ID_PPB_AUDIO, resource, true));
  }

  void StopPlayback(PP_Resource resource) {
    if (!playing())
      return;
    PluginDispatcher::GetForInstance(instance())->Send(
        new PpapiHostMsg_PPBAudio_StartOrStop(
            INTERFACE_ID_PPB_AUDIO, resource, false));
    SetStopPlaybackState();
  }

 private:
  PP_Resource config_;

  DISALLOW_COPY_AND_ASSIGN(Audio);
};

namespace {

PP_Resource Create(PP_Instance instance_id,
                   PP_Resource config_id,
                   PPB_Audio_Callback callback,
                   void* user_data) {
  PP_Resource result;
  PluginDispatcher::Get()->Send(new PpapiHostMsg_PPBAudio_Create(
      INTERFACE_ID_PPB_AUDIO, instance_id, config_id, &result));
  if (!result)
    return 0;

  linked_ptr<Audio> object(new Audio(instance_id, config_id,
                                     callback, user_data));
  PluginResourceTracker::GetInstance()->AddResource(result, object);
  return result;
}

PP_Bool IsAudio(PP_Resource resource) {
  Audio* object = PluginResource::GetAs<Audio>(resource);
  return BoolToPPBool(!!object);
}

PP_Resource GetCurrentConfiguration(PP_Resource audio_id) {
  Audio* object = PluginResource::GetAs<Audio>(audio_id);
  if (!object)
    return 0;
  PP_Resource result = object->config();
  PluginResourceTracker::GetInstance()->AddRefResource(result);
  return result;
}

PP_Bool StartPlayback(PP_Resource audio_id) {
  Audio* object = PluginResource::GetAs<Audio>(audio_id);
  if (!object)
    return PP_FALSE;
  object->StartPlayback(audio_id);
  return PP_TRUE;
}

PP_Bool StopPlayback(PP_Resource audio_id) {
  Audio* object = PluginResource::GetAs<Audio>(audio_id);
  if (!object)
    return PP_FALSE;
  object->StopPlayback(audio_id);
  return PP_TRUE;
}

const PPB_Audio audio_interface = {
  &Create,
  &IsAudio,
  &GetCurrentConfiguration,
  &StartPlayback,
  &StopPlayback
};

}  // namespace

PPB_Audio_Proxy::PPB_Audio_Proxy(Dispatcher* dispatcher,
                                 const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

PPB_Audio_Proxy::~PPB_Audio_Proxy() {
}

const void* PPB_Audio_Proxy::GetSourceInterface() const {
  return &audio_interface;
}

InterfaceID PPB_Audio_Proxy::GetInterfaceId() const {
  return INTERFACE_ID_PPB_AUDIO;
}

bool PPB_Audio_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Audio_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBAudio_Create, OnMsgCreate)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBAudio_StartOrStop,
                        OnMsgStartOrStop)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBAudio_NotifyAudioStreamCreated,
                        OnMsgNotifyAudioStreamCreated)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPB_Audio_Proxy::OnMsgCreate(PP_Instance instance_id,
                                  PP_Resource config_id,
                                  PP_Resource* result) {
  const PPB_AudioTrusted* audio_trusted =
      reinterpret_cast<const PPB_AudioTrusted*>(
          dispatcher()->GetLocalInterface(PPB_AUDIO_TRUSTED_INTERFACE));
  if (!audio_trusted) {
    *result = 0;
    return;
  }

  *result = audio_trusted->CreateTrusted(instance_id);
  if (!result)
    return;

  CompletionCallback callback = callback_factory_.NewCallback(
      &PPB_Audio_Proxy::AudioChannelConnected, *result);
  int32_t open_error = audio_trusted->Open(*result, config_id,
                                           callback.pp_completion_callback());
  if (open_error != PP_ERROR_WOULDBLOCK)
    callback.Run(open_error);
}

void PPB_Audio_Proxy::OnMsgStartOrStop(PP_Resource audio_id, bool play) {
  if (play)
    ppb_audio_target()->StartPlayback(audio_id);
  else
    ppb_audio_target()->StopPlayback(audio_id);
}

void PPB_Audio_Proxy::OnMsgNotifyAudioStreamCreated(
    PP_Resource audio_id,
    int32_t result_code,
    IPC::PlatformFileForTransit socket_handle,
    base::SharedMemoryHandle handle,
    uint32_t length) {
  Audio* object = PluginResource::GetAs<Audio>(audio_id);
  if (!object || result_code != PP_OK) {
    // The caller may still have given us these handles in the failure case.
    // The easiest way to clean these up is to just put them in the objects
    // and then close them. This failure case is not performance critical.
    base::SyncSocket temp_socket(
        IPC::PlatformFileForTransitToPlatformFile(socket_handle));
    base::SharedMemory temp_mem(handle, false);
    return;
  }
  object->SetStreamInfo(
      handle, length, IPC::PlatformFileForTransitToPlatformFile(socket_handle));
}

void PPB_Audio_Proxy::AudioChannelConnected(int32_t result,
                                            PP_Resource resource) {
  IPC::PlatformFileForTransit socket_handle =
      IPC::InvalidPlatformFileForTransit();
#if defined(OS_WIN)
  base::SharedMemoryHandle shared_memory = NULL;
#elif defined(OS_POSIX)
  base::SharedMemoryHandle shared_memory(-1, false);
#else
  #error Not implemented.
#endif
  uint32_t shared_memory_length = 0;

  int32_t result_code = result;
  if (result_code == PP_OK) {
    result_code = GetAudioConnectedHandles(resource, &socket_handle,
                                           &shared_memory,
                                           &shared_memory_length);
  }

  // Send all the values, even on error. This simplifies some of our cleanup
  // code since the handles will be in the other process and could be
  // inconvenient to clean up. Our IPC code will automatically handle this for
  // us, as long as the remote side always closes the handles it receives
  // (in OnMsgNotifyAudioStreamCreated), even in the failure case.
  dispatcher()->Send(new PpapiMsg_PPBAudio_NotifyAudioStreamCreated(
      INTERFACE_ID_PPB_AUDIO, resource, result_code, socket_handle,
      shared_memory, shared_memory_length));
}

int32_t PPB_Audio_Proxy::GetAudioConnectedHandles(
    PP_Resource resource,
    IPC::PlatformFileForTransit* foreign_socket_handle,
    base::SharedMemoryHandle* foreign_shared_memory_handle,
    uint32_t* shared_memory_length) {
  // Get the trusted audio interface which will give us the handles.
  const PPB_AudioTrusted* audio_trusted =
      reinterpret_cast<const PPB_AudioTrusted*>(
          dispatcher()->GetLocalInterface(PPB_AUDIO_TRUSTED_INTERFACE));
  if (!audio_trusted)
    return PP_ERROR_NOINTERFACE;

  // Get the socket handle for signaling.
  int32_t socket_handle;
  int32_t result = audio_trusted->GetSyncSocket(resource, &socket_handle);
  if (result != PP_OK)
    return result;

#if defined(OS_WIN)
  // On Windows, duplicate the socket into the plugin process, this will
  // automatically close the source handle.
  ::DuplicateHandle(
      GetCurrentProcess(),
      reinterpret_cast<HANDLE>(static_cast<intptr_t>(socket_handle)),
      dispatcher()->remote_process_handle(), foreign_socket_handle,
      STANDARD_RIGHTS_REQUIRED | FILE_MAP_READ | FILE_MAP_WRITE,
      FALSE, DUPLICATE_CLOSE_SOURCE);
#else
  // On Posix, the socket handle will be auto-duplicated when we send the
  // FileDescriptor. Set AutoClose since we don't need the handle any more.
  *foreign_socket_handle = base::FileDescriptor(socket_handle, true);
#endif

  // Get the shared memory for the buffer.
  // TODO(brettw) remove the reinterpret cast when the interface is updated.
  int shared_memory_handle;
  result = audio_trusted->GetSharedMemory(resource, &shared_memory_handle,
      shared_memory_length);
  if (result != PP_OK)
    return result;

  base::SharedMemory shared_memory(
#if defined(OS_WIN)
      reinterpret_cast<HANDLE>(static_cast<intptr_t>(shared_memory_handle)),
#else
      base::FileDescriptor(shared_memory_handle, false),
#endif
      false);

  // Duplicate the shared memory to the plugin process. This will automatically
  // close the source handle.
  if (!shared_memory.GiveToProcess(dispatcher()->remote_process_handle(),
                                   foreign_shared_memory_handle))
    return PP_ERROR_FAILED;

  return PP_OK;
}

}  // namespace proxy
}  // namespace pp
