// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_AUDIO_INPUT_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_AUDIO_INPUT_IMPL_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/shared_memory.h"
#include "base/sync_socket.h"
#include "ppapi/c/dev/ppb_audio_input_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/trusted/ppb_audio_input_trusted_dev.h"
#include "ppapi/shared_impl/audio_config_impl.h"
#include "ppapi/shared_impl/audio_input_impl.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/scoped_pp_resource.h"
#include "webkit/plugins/ppapi/audio_helper.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

namespace webkit {
namespace ppapi {

// Some of the backend functionality of this class is implemented by the
// AudioInputImpl so it can be shared with the proxy.
class PPB_AudioInput_Impl : public ::ppapi::Resource,
                            public ::ppapi::AudioInputImpl,
                            public AudioHelper {
 public:
  // Trusted initialization. You must call Init after this.
  //
  // Untrusted initialization should just call the static Create() function
  // to properly create & initialize this class.
  explicit PPB_AudioInput_Impl(PP_Instance instance);

  virtual ~PPB_AudioInput_Impl();

  // Creation function for untrusted plugins. This handles all initialization
  // and will return 0 on failure.
  static PP_Resource Create(PP_Instance instance,
                            PP_Resource config_id,
                            PPB_AudioInput_Callback audio_input_callback,
                            void* user_data);

  // Initialization function for non-trusted init.
  bool Init(PP_Resource config_id,
      PPB_AudioInput_Callback callback, void* user_data);

  // Resource overrides.
  virtual ::ppapi::thunk::PPB_AudioInput_API* AsPPB_AudioInput_API() OVERRIDE;

  // PPB_AudioInput_API implementation.
  virtual PP_Resource GetCurrentConfig() OVERRIDE;
  virtual PP_Bool StartCapture() OVERRIDE;
  virtual PP_Bool StopCapture() OVERRIDE;

  virtual int32_t OpenTrusted(PP_Resource config_id,
                              PP_CompletionCallback create_callback) OVERRIDE;
  virtual int32_t GetSyncSocket(int* sync_socket) OVERRIDE;
  virtual int32_t GetSharedMemory(int* shm_handle, uint32_t* shm_size) OVERRIDE;

 private:
  // AudioHelper implementation.
  virtual void OnSetStreamInfo(base::SharedMemoryHandle shared_memory_handle,
                               size_t shared_memory_size_,
                               base::SyncSocket::Handle socket) OVERRIDE;

  // AudioConfig used for creating this AudioInput object. We own a ref.
  ::ppapi::ScopedPPResource config_;

  // PluginDelegate audio input object that we delegate audio IPC through.
  // We don't own this pointer but are responsible for calling Shutdown on it.
  PluginDelegate::PlatformAudioInput* audio_input_;

  DISALLOW_COPY_AND_ASSIGN(PPB_AudioInput_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_AUDIO_INPUT_IMPL_H_
