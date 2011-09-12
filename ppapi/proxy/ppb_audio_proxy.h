// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_AUDIO_PROXY_H_
#define PPAPI_PROXY_PPB_AUDIO_PROXY_H_

#include <utility>

#include "base/basictypes.h"
#include "base/shared_memory.h"
#include "base/sync_socket.h"
#include "ipc/ipc_platform_file.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/proxy_non_thread_safe_ref_count.h"

struct PPB_Audio;

namespace ppapi {

class HostResource;

namespace proxy {

class PPB_Audio_Proxy : public InterfaceProxy {
 public:
  PPB_Audio_Proxy(Dispatcher* dispatcher, const void* target_interface);
  virtual ~PPB_Audio_Proxy();

  static const Info* GetInfo();

  // Creates an Audio object in the plugin process.
  static PP_Resource CreateProxyResource(PP_Instance instance_id,
                                         PP_Resource config_id,
                                         PPB_Audio_Callback audio_callback,
                                         void* user_data);


  const PPB_Audio* ppb_audio_target() const {
    return static_cast<const PPB_Audio*>(target_interface());
  }

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  // Plugin->renderer message handlers.
  void OnMsgCreate(PP_Instance instance_id,
                   int32_t sample_rate,
                   uint32_t sample_frame_count,
                   ppapi::HostResource* result);
  void OnMsgStartOrStop(const ppapi::HostResource& audio_id, bool play);

  // Renderer->plugin message handlers.
  void OnMsgNotifyAudioStreamCreated(const ppapi::HostResource& audio_id,
                                     int32_t result_code,
                                     IPC::PlatformFileForTransit socket_handle,
                                     base::SharedMemoryHandle handle,
                                     uint32_t length);

  void AudioChannelConnected(int32_t result,
                             const ppapi::HostResource& resource);

  // In the renderer, this is called in response to a stream created message.
  // It will retrieve the shared memory and socket handles and place them into
  // the given out params. The return value is a PPAPI error code.
  //
  // The input arguments should be initialized to 0 or -1, depending on the
  // platform's default invalid handle values. On error, some of these
  // arguments may be written to, and others may be untouched, depending on
  // where the error occurred.
  int32_t GetAudioConnectedHandles(
      const ppapi::HostResource& resource,
      IPC::PlatformFileForTransit* foreign_socket_handle,
      base::SharedMemoryHandle* foreign_shared_memory_handle,
      uint32_t* shared_memory_length);

  pp::CompletionCallbackFactory<PPB_Audio_Proxy,
                                ProxyNonThreadSafeRefCount> callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Audio_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_AUDIO_PROXY_H_
