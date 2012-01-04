// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PPB_AUDIO_INPUT_PROXY_H_
#define PPAPI_PPB_AUDIO_INPUT_PROXY_H_

#include <utility>

#include "base/basictypes.h"
#include "base/shared_memory.h"
#include "base/sync_socket.h"
#include "ipc/ipc_platform_file.h"
#include "ppapi/c/dev/ppb_audio_input_dev.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/proxy_non_thread_safe_ref_count.h"

struct PPB_AudioInput_Dev;

namespace ppapi {

class HostResource;

namespace proxy {

class PPB_AudioInput_Proxy : public InterfaceProxy {
 public:
  explicit PPB_AudioInput_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_AudioInput_Proxy();

  static PP_Resource CreateProxyResource(
      PP_Instance instance,
      PP_Resource config_id,
      PPB_AudioInput_Callback audio_input_callback,
      void* user_data);

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  static const ApiID kApiID = API_ID_PPB_AUDIO_INPUT_DEV;

 private:
  // Message handlers.
  // Plugin->renderer message handlers.
  void OnMsgCreate(PP_Instance instance_id,
                   int32_t sample_rate,
                   uint32_t sample_frame_count,
                   ppapi::HostResource* result);
  void OnMsgStartOrStop(const ppapi::HostResource& audio_id, bool capture);

  // Renderer->plugin message handlers.
  void OnMsgNotifyAudioStreamCreated(const ppapi::HostResource& audio_id,
                                     int32_t result_code,
                                     IPC::PlatformFileForTransit socket_handle,
                                     base::SharedMemoryHandle handle,
                                     uint32_t length);

  void AudioInputChannelConnected(int32_t result,
                                  const ppapi::HostResource& resource);

  // In the renderer, this is called in response to a stream created message.
  // It will retrieve the shared memory and socket handles and place them into
  // the given out params. The return value is a PPAPI error code.
  //
  // The input arguments should be initialized to 0 or -1, depending on the
  // platform's default invalid handle values. On error, some of these
  // arguments may be written to, and others may be untouched, depending on
  // where the error occurred.
  int32_t GetAudioInputConnectedHandles(
      const ppapi::HostResource& resource,
      IPC::PlatformFileForTransit* foreign_socket_handle,
      base::SharedMemoryHandle* foreign_shared_memory_handle,
      uint32_t* shared_memory_length);

  pp::CompletionCallbackFactory<PPB_AudioInput_Proxy,
                                ProxyNonThreadSafeRefCount> callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(PPB_AudioInput_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PPB_AUDIO_INPUT_PROXY_H_
