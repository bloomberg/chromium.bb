// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_audio_input_proxy.h"

#include "base/compiler_specific.h"
#include "ppapi/c/dev/ppb_audio_input_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/proxy/enter_proxy.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/api_id.h"
#include "ppapi/shared_impl/platform_file.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/ppb_audio_input_shared.h"
#include "ppapi/shared_impl/ppb_device_ref_shared.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

using ppapi::IntToPlatformFile;
using ppapi::thunk::PPB_AudioInput_API;

namespace ppapi {
namespace proxy {

class AudioInput : public PPB_AudioInput_Shared {
 public:
  explicit AudioInput(const HostResource& audio_input);
  virtual ~AudioInput();

  // Implementation of PPB_AudioInput_API trusted methods.
  virtual int32_t OpenTrusted(
      const std::string& device_id,
      PP_Resource config,
      scoped_refptr<TrackedCallback> create_callback) OVERRIDE;
  virtual int32_t GetSyncSocket(int* sync_socket) OVERRIDE;
  virtual int32_t GetSharedMemory(int* shm_handle, uint32_t* shm_size) OVERRIDE;
  virtual const std::vector<DeviceRefData>& GetDeviceRefData() const OVERRIDE;

 private:
  // PPB_AudioInput_Shared implementation.
  virtual int32_t InternalEnumerateDevices(
      PP_Resource* devices,
      scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t InternalOpen(
      const std::string& device_id,
      PP_AudioSampleRate sample_rate,
      uint32_t sample_frame_count,
      scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual PP_Bool InternalStartCapture() OVERRIDE;
  virtual PP_Bool InternalStopCapture() OVERRIDE;
  virtual void InternalClose() OVERRIDE;

  PluginDispatcher* GetDispatcher() const {
    return PluginDispatcher::GetForResource(this);
  }

  DISALLOW_COPY_AND_ASSIGN(AudioInput);
};

AudioInput::AudioInput(const HostResource& audio_input)
    : PPB_AudioInput_Shared(audio_input) {
}

AudioInput::~AudioInput() {
  Close();
}

int32_t AudioInput::OpenTrusted(
    const std::string& device_id,
    PP_Resource config,
    scoped_refptr<TrackedCallback> create_callback) {
  return PP_ERROR_NOTSUPPORTED;  // Don't proxy the trusted interface.
}

int32_t AudioInput::GetSyncSocket(int* sync_socket) {
  return PP_ERROR_NOTSUPPORTED;  // Don't proxy the trusted interface.
}

int32_t AudioInput::GetSharedMemory(int* shm_handle, uint32_t* shm_size) {
  return PP_ERROR_NOTSUPPORTED;  // Don't proxy the trusted interface.
}

const std::vector<DeviceRefData>& AudioInput::GetDeviceRefData() const {
  // Don't proxy the trusted interface.
  static std::vector<DeviceRefData> result;
  return result;
}

int32_t AudioInput::InternalEnumerateDevices(
    PP_Resource* devices,
    scoped_refptr<TrackedCallback> callback) {
  devices_ = devices;
  enumerate_devices_callback_ = callback;
  GetDispatcher()->Send(new PpapiHostMsg_PPBAudioInput_EnumerateDevices(
      API_ID_PPB_AUDIO_INPUT_DEV, host_resource()));
  return PP_OK_COMPLETIONPENDING;
}

int32_t AudioInput::InternalOpen(const std::string& device_id,
                                 PP_AudioSampleRate sample_rate,
                                 uint32_t sample_frame_count,
                                 scoped_refptr<TrackedCallback> callback) {
  open_callback_ = callback;
  GetDispatcher()->Send(new PpapiHostMsg_PPBAudioInput_Open(
      API_ID_PPB_AUDIO_INPUT_DEV, host_resource(), device_id, sample_rate,
      sample_frame_count));
  return PP_OK_COMPLETIONPENDING;
}

PP_Bool AudioInput::InternalStartCapture() {
  SetStartCaptureState();
  GetDispatcher()->Send(new PpapiHostMsg_PPBAudioInput_StartOrStop(
      API_ID_PPB_AUDIO_INPUT_DEV, host_resource(), true));
  return PP_TRUE;
}

PP_Bool AudioInput::InternalStopCapture() {
  GetDispatcher()->Send(new PpapiHostMsg_PPBAudioInput_StartOrStop(
      API_ID_PPB_AUDIO_INPUT_DEV, host_resource(), false));
  SetStopCaptureState();
  return PP_TRUE;
}

void AudioInput::InternalClose() {
  GetDispatcher()->Send(new PpapiHostMsg_PPBAudioInput_Close(
      API_ID_PPB_AUDIO_INPUT_DEV, host_resource()));
}

PPB_AudioInput_Proxy::PPB_AudioInput_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

PPB_AudioInput_Proxy::~PPB_AudioInput_Proxy() {
}

// static
PP_Resource PPB_AudioInput_Proxy::CreateProxyResource0_1(
    PP_Instance instance,
    PP_Resource config,
    PPB_AudioInput_Callback audio_input_callback,
    void* user_data) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return 0;

  HostResource result;
  dispatcher->Send(new PpapiHostMsg_PPBAudioInput_Create(
      API_ID_PPB_AUDIO_INPUT_DEV, instance, &result));
  if (result.is_null())
    return 0;

  AudioInput* audio_input = new AudioInput(result);
  int32_t open_result = audio_input->Open("", config, audio_input_callback,
      user_data, AudioInput::MakeIgnoredCompletionCallback(audio_input));
  if (open_result != PP_OK && open_result != PP_OK_COMPLETIONPENDING) {
    delete audio_input;
    return 0;
  }
  return audio_input->GetReference();
}

// static
PP_Resource PPB_AudioInput_Proxy::CreateProxyResource(
    PP_Instance instance) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return 0;

  HostResource result;
  dispatcher->Send(new PpapiHostMsg_PPBAudioInput_Create(
      API_ID_PPB_AUDIO_INPUT_DEV, instance, &result));
  if (result.is_null())
    return 0;

  return (new AudioInput(result))->GetReference();
}

bool PPB_AudioInput_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_AudioInput_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBAudioInput_Create, OnMsgCreate)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBAudioInput_EnumerateDevices,
                        OnMsgEnumerateDevices)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBAudioInput_Open, OnMsgOpen)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBAudioInput_StartOrStop,
                        OnMsgStartOrStop)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBAudioInput_Close, OnMsgClose)

    IPC_MESSAGE_HANDLER(PpapiMsg_PPBAudioInput_EnumerateDevicesACK,
                        OnMsgEnumerateDevicesACK)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBAudioInput_OpenACK, OnMsgOpenACK)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  // TODO(brettw) handle bad messages!

  return handled;
}

void PPB_AudioInput_Proxy::OnMsgCreate(PP_Instance instance,
                                       HostResource* result) {
  thunk::EnterResourceCreation resource_creation(instance);
  if (resource_creation.succeeded()) {
    result->SetHostResource(
        instance, resource_creation.functions()->CreateAudioInput(instance));
  }
}

void PPB_AudioInput_Proxy::OnMsgEnumerateDevices(
    const ppapi::HostResource& audio_input) {
  EnterHostFromHostResourceForceCallback<PPB_AudioInput_API> enter(
      audio_input, callback_factory_,
      &PPB_AudioInput_Proxy::EnumerateDevicesACKInHost, audio_input);

  if (enter.succeeded())
    enter.SetResult(enter.object()->EnumerateDevices(NULL, enter.callback()));
}

void PPB_AudioInput_Proxy::OnMsgOpen(const ppapi::HostResource& audio_input,
                                     const std::string& device_id,
                                     int32_t sample_rate,
                                     uint32_t sample_frame_count) {
  // The ...ForceCallback class will help ensure the callback is always called.
  // All error cases must call SetResult on this class.
  EnterHostFromHostResourceForceCallback<PPB_AudioInput_API> enter(
      audio_input, callback_factory_, &PPB_AudioInput_Proxy::OpenACKInHost,
      audio_input);
  if (enter.failed())
    return;  // When enter fails, it will internally schedule the callback.

  thunk::EnterResourceCreation resource_creation(audio_input.instance());
  // Make an audio config object.
  PP_Resource audio_config_res =
      resource_creation.functions()->CreateAudioConfig(
          audio_input.instance(), static_cast<PP_AudioSampleRate>(sample_rate),
          sample_frame_count);
  if (!audio_config_res) {
    enter.SetResult(PP_ERROR_FAILED);
    return;
  }

  // Initiate opening the audio object.
  enter.SetResult(enter.object()->OpenTrusted(
      device_id, audio_config_res, enter.callback()));

  // Clean up the temporary audio config resource we made.
  const PPB_Core* core = static_cast<const PPB_Core*>(
      dispatcher()->local_get_interface()(PPB_CORE_INTERFACE));
  core->ReleaseResource(audio_config_res);
}

void PPB_AudioInput_Proxy::OnMsgStartOrStop(const HostResource& audio_input,
                                            bool capture) {
  EnterHostFromHostResource<PPB_AudioInput_API> enter(audio_input);
  if (enter.failed())
    return;
  if (capture)
    enter.object()->StartCapture();
  else
    enter.object()->StopCapture();
}

void PPB_AudioInput_Proxy::OnMsgClose(const ppapi::HostResource& audio_input) {
  EnterHostFromHostResource<PPB_AudioInput_API> enter(audio_input);
  if (enter.succeeded())
    enter.object()->Close();
}

// Processed in the plugin (message from host).
void PPB_AudioInput_Proxy::OnMsgEnumerateDevicesACK(
    const ppapi::HostResource& audio_input,
    int32_t result,
    const std::vector<ppapi::DeviceRefData>& devices) {
  EnterPluginFromHostResource<PPB_AudioInput_API> enter(audio_input);
  if (enter.succeeded()) {
    static_cast<AudioInput*>(enter.object())->OnEnumerateDevicesComplete(
        result, devices);
  }
}

void PPB_AudioInput_Proxy::OnMsgOpenACK(
    const HostResource& audio_input,
    int32_t result,
    IPC::PlatformFileForTransit socket_handle,
    base::SharedMemoryHandle handle,
    uint32_t length) {
  EnterPluginFromHostResource<PPB_AudioInput_API> enter(audio_input);
  if (enter.failed()) {
    // The caller may still have given us these handles in the failure case.
    // The easiest way to clean these up is to just put them in the objects
    // and then close them. This failure case is not performance critical.
    base::SyncSocket temp_socket(
        IPC::PlatformFileForTransitToPlatformFile(socket_handle));
    base::SharedMemory temp_mem(handle, false);
  } else {
    static_cast<AudioInput*>(enter.object())->OnOpenComplete(
        result, handle, length,
        IPC::PlatformFileForTransitToPlatformFile(socket_handle));
  }
}

void PPB_AudioInput_Proxy::EnumerateDevicesACKInHost(
    int32_t result,
    const HostResource& audio_input) {
  EnterHostFromHostResource<PPB_AudioInput_API> enter(audio_input);
  dispatcher()->Send(new PpapiMsg_PPBAudioInput_EnumerateDevicesACK(
      API_ID_PPB_AUDIO_INPUT_DEV, audio_input, result,
      enter.succeeded() && result == PP_OK ?
          enter.object()->GetDeviceRefData() : std::vector<DeviceRefData>()));
}

void PPB_AudioInput_Proxy::OpenACKInHost(int32_t result,
                                         const HostResource& audio_input) {
  IPC::PlatformFileForTransit socket_handle =
      IPC::InvalidPlatformFileForTransit();
  base::SharedMemoryHandle shared_memory = IPC::InvalidPlatformFileForTransit();
  uint32_t shared_memory_length = 0;

  if (result == PP_OK) {
    result = GetAudioInputConnectedHandles(audio_input, &socket_handle,
                                           &shared_memory,
                                           &shared_memory_length);
  }

  // Send all the values, even on error. This simplifies some of our cleanup
  // code since the handles will be in the other process and could be
  // inconvenient to clean up. Our IPC code will automatically handle this for
  // us, as long as the remote side always closes the handles it receives
  // (in OnMsgOpenACK), even in the failure case.
  dispatcher()->Send(new PpapiMsg_PPBAudioInput_OpenACK(
      API_ID_PPB_AUDIO_INPUT_DEV, audio_input, result, socket_handle,
      shared_memory, shared_memory_length));
}

int32_t PPB_AudioInput_Proxy::GetAudioInputConnectedHandles(
    const HostResource& resource,
    IPC::PlatformFileForTransit* foreign_socket_handle,
    base::SharedMemoryHandle* foreign_shared_memory_handle,
    uint32_t* shared_memory_length) {
  // Get the audio interface which will give us the handles.
  EnterHostFromHostResource<PPB_AudioInput_API> enter(resource);
  if (enter.failed())
    return PP_ERROR_NOINTERFACE;

  // Get the socket handle for signaling.
  int32_t socket_handle;
  int32_t result = enter.object()->GetSyncSocket(&socket_handle);
  if (result != PP_OK)
    return result;

  // socket_handle doesn't belong to us: don't close it.
  *foreign_socket_handle = dispatcher()->ShareHandleWithRemote(
      IntToPlatformFile(socket_handle), false);
  if (*foreign_socket_handle == IPC::InvalidPlatformFileForTransit())
    return PP_ERROR_FAILED;

  // Get the shared memory for the buffer.
  int shared_memory_handle;
  result = enter.object()->GetSharedMemory(&shared_memory_handle,
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
}  // namespace ppapi
