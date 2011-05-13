// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_audio_proxy.h"

#include "base/threading/simple_thread.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/trusted/ppb_audio_trusted.h"
#include "ppapi/proxy/interface_id.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/audio_impl.h"
#include "ppapi/thunk/ppb_audio_config_api.h"
#include "ppapi/thunk/ppb_audio_trusted_api.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace pp {
namespace proxy {

class Audio : public PluginResource, public ppapi::AudioImpl {
 public:
  Audio(const HostResource& audio_id,
        PP_Resource config_id,
        PPB_Audio_Callback callback,
        void* user_data);
  virtual ~Audio();

  // ResourceObjectBase overrides.
  virtual ::ppapi::thunk::PPB_Audio_API* AsAudio_API();

  // PPB_Audio_API implementation.
  virtual PP_Resource GetCurrentConfig() OVERRIDE;
  virtual PP_Bool StartPlayback() OVERRIDE;
  virtual PP_Bool StopPlayback() OVERRIDE;

 private:
  // Owning reference to the current config object. This isn't actually used,
  // we just dish it out as requested by the plugin.
  PP_Resource config_;

  DISALLOW_COPY_AND_ASSIGN(Audio);
};

Audio::Audio(const HostResource& audio_id,
             PP_Resource config_id,
             PPB_Audio_Callback callback,
             void* user_data)
    : PluginResource(audio_id),
      config_(config_id) {
  SetCallback(callback, user_data);
  PluginResourceTracker::GetInstance()->AddRefResource(config_);
}

Audio::~Audio() {
  PluginResourceTracker::GetInstance()->ReleaseResource(config_);
}

::ppapi::thunk::PPB_Audio_API* Audio::AsAudio_API() {
  return this;
}

PP_Resource Audio::GetCurrentConfig() {
  // AddRef for the caller.
  PluginResourceTracker::GetInstance()->AddRefResource(config_);
  return config_;
}

PP_Bool Audio::StartPlayback() {
  if (playing())
    return PP_TRUE;
  SetStartPlaybackState();
  PluginDispatcher::GetForInstance(instance())->Send(
      new PpapiHostMsg_PPBAudio_StartOrStop(
          INTERFACE_ID_PPB_AUDIO, host_resource(), true));
  return PP_TRUE;
}

PP_Bool Audio::StopPlayback() {
  if (!playing())
    return PP_TRUE;
  PluginDispatcher::GetForInstance(instance())->Send(
      new PpapiHostMsg_PPBAudio_StartOrStop(
          INTERFACE_ID_PPB_AUDIO, host_resource(), false));
  SetStopPlaybackState();
  return PP_TRUE;
}

namespace {

InterfaceProxy* CreateAudioProxy(Dispatcher* dispatcher,
                                 const void* target_interface) {
  return new PPB_Audio_Proxy(dispatcher, target_interface);
}

base::PlatformFile IntToPlatformFile(int32_t handle) {
  // TODO(piman/brettw): Change trusted interface to return a PP_FileHandle,
  // those casts are ugly.
#if defined(OS_WIN)
  return reinterpret_cast<HANDLE>(static_cast<intptr_t>(handle));
#elif defined(OS_POSIX)
  return handle;
#else
  #error Not implemented.
#endif
}

}  // namespace

PPB_Audio_Proxy::PPB_Audio_Proxy(Dispatcher* dispatcher,
                                 const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

PPB_Audio_Proxy::~PPB_Audio_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_Audio_Proxy::GetInfo() {
  static const Info info = {
    ppapi::thunk::GetPPB_Audio_Thunk(),
    PPB_AUDIO_INTERFACE,
    INTERFACE_ID_PPB_AUDIO,
    false,
    &CreateAudioProxy,
  };
  return &info;
}

// static
PP_Resource PPB_Audio_Proxy::CreateProxyResource(
    PP_Instance instance_id,
    PP_Resource config_id,
    PPB_Audio_Callback audio_callback,
    void* user_data) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance_id);
  if (!dispatcher)
    return 0;

  ::ppapi::thunk::EnterResourceNoLock< ::ppapi::thunk::PPB_AudioConfig_API>
      config(config_id, true);
  if (config.failed())
    return 0;

  HostResource result;
  dispatcher->Send(new PpapiHostMsg_PPBAudio_Create(
      INTERFACE_ID_PPB_AUDIO, instance_id,
      config.object()->GetSampleRate(), config.object()->GetSampleFrameCount(),
      &result));
  if (result.is_null())
    return 0;

  linked_ptr<Audio> object(new Audio(result, config_id,
                                     audio_callback, user_data));
  return PluginResourceTracker::GetInstance()->AddResource(object);
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
                                  int32_t sample_rate,
                                  uint32_t sample_frame_count,
                                  HostResource* result) {
  ::ppapi::thunk::EnterFunction< ::ppapi::thunk::ResourceCreationAPI>
      resource_creation(instance_id, true);
  if (resource_creation.failed())
    return;

  // Make the resource and get the API pointer to its trusted interface.
  result->SetHostResource(
      instance_id,
      resource_creation.functions()->CreateAudioTrusted(instance_id));
  if (result->is_null())
    return;
  ::ppapi::thunk::EnterResourceNoLock< ::ppapi::thunk::PPB_AudioTrusted_API>
      trusted_audio(result->host_resource(), false);
  if (trusted_audio.failed())
    return;

  // Make an audio config object.
  PP_Resource audio_config_res =
      resource_creation.functions()->CreateAudioConfig(
          instance_id, static_cast<PP_AudioSampleRate>(sample_rate),
          sample_frame_count);
  if (!audio_config_res)
    return;

  // Initiate opening the audio object.
  CompletionCallback callback = callback_factory_.NewCallback(
      &PPB_Audio_Proxy::AudioChannelConnected, *result);
  int32_t open_error = trusted_audio.object()->OpenTrusted(
      audio_config_res, callback.pp_completion_callback());
  if (open_error != PP_OK_COMPLETIONPENDING)
    callback.Run(open_error);

  // Clean up the temporary audio config resource we made.
  const PPB_Core* core = static_cast<const PPB_Core*>(
      dispatcher()->GetLocalInterface(PPB_CORE_INTERFACE));
  core->ReleaseResource(audio_config_res);
}

void PPB_Audio_Proxy::OnMsgStartOrStop(const HostResource& audio_id,
                                       bool play) {
  if (play)
    ppb_audio_target()->StartPlayback(audio_id.host_resource());
  else
    ppb_audio_target()->StopPlayback(audio_id.host_resource());
}

// Processed in the plugin (message from host).
void PPB_Audio_Proxy::OnMsgNotifyAudioStreamCreated(
    const HostResource& audio_id,
    int32_t result_code,
    IPC::PlatformFileForTransit socket_handle,
    base::SharedMemoryHandle handle,
    uint32_t length) {
  PP_Resource plugin_resource =
      PluginResourceTracker::GetInstance()->PluginResourceForHostResource(
          audio_id);
  ppapi::thunk::EnterResource<ppapi::thunk::PPB_Audio_API> enter(
      plugin_resource, false);
  if (enter.failed() || result_code != PP_OK) {
    // The caller may still have given us these handles in the failure case.
    // The easiest way to clean these up is to just put them in the objects
    // and then close them. This failure case is not performance critical.
    base::SyncSocket temp_socket(
        IPC::PlatformFileForTransitToPlatformFile(socket_handle));
    base::SharedMemory temp_mem(handle, false);
    return;
  }
  Audio* audio = static_cast<Audio*>(enter.object());
  audio->SetStreamInfo(
      handle, length, IPC::PlatformFileForTransitToPlatformFile(socket_handle));
}

void PPB_Audio_Proxy::AudioChannelConnected(
    int32_t result,
    const HostResource& resource) {
  IPC::PlatformFileForTransit socket_handle =
      IPC::InvalidPlatformFileForTransit();
  base::SharedMemoryHandle shared_memory = IPC::InvalidPlatformFileForTransit();
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
    const HostResource& resource,
    IPC::PlatformFileForTransit* foreign_socket_handle,
    base::SharedMemoryHandle* foreign_shared_memory_handle,
    uint32_t* shared_memory_length) {
  // Get the trusted audio interface which will give us the handles.
  ::ppapi::thunk::EnterResourceNoLock< ::ppapi::thunk::PPB_AudioTrusted_API>
      trusted_audio(resource.host_resource(), false);
  if (trusted_audio.failed())
    return PP_ERROR_NOINTERFACE;

  // Get the socket handle for signaling.
  int32_t socket_handle;
  int32_t result = trusted_audio.object()->GetSyncSocket(&socket_handle);
  if (result != PP_OK)
    return result;

  // socket_handle doesn't belong to us: don't close it.
  *foreign_socket_handle = dispatcher()->ShareHandleWithRemote(
      IntToPlatformFile(socket_handle), false);
  if (*foreign_socket_handle == IPC::InvalidPlatformFileForTransit())
    return PP_ERROR_FAILED;

  // Get the shared memory for the buffer.
  int shared_memory_handle;
  result = trusted_audio.object()->GetSharedMemory(&shared_memory_handle,
                                                   shared_memory_length);
  if (result != PP_OK)
    return result;

  // shared_memory_handle doesn't belong to us: don't close it.
  *foreign_shared_memory_handle = dispatcher()->ShareHandleWithRemote(
      IntToPlatformFile(shared_memory_handle), false);
  if (*foreign_shared_memory_handle == IPC::InvalidPlatformFileForTransit())
    return PP_ERROR_FAILED;

  return PP_OK;
}

}  // namespace proxy
}  // namespace pp
