// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_AUDIO_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_AUDIO_IMPL_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/shared_memory.h"
#include "base/sync_socket.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/c/trusted/ppb_audio_trusted.h"
#include "ppapi/shared_impl/ppb_audio_config_shared.h"
#include "ppapi/shared_impl/ppb_audio_shared.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/scoped_pp_resource.h"
#include "webkit/plugins/ppapi/audio_helper.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

namespace webkit {
namespace ppapi {

// Some of the backend functionality of this class is implemented by the
// PPB_Audio_Shared so it can be shared with the proxy.
class PPB_Audio_Impl : public ::ppapi::Resource,
                       public ::ppapi::PPB_Audio_Shared,
                       public AudioHelper {
 public:
  // Trusted initialization. You must call Init after this.
  //
  // Untrusted initialization should just call the static Create() function
  // to properly create & initialize this class.
  explicit PPB_Audio_Impl(PP_Instance instance);

  virtual ~PPB_Audio_Impl();

  // Creation function for untrusted plugins. This handles all initialization
  // and will return 0 on failure.
  static PP_Resource Create(PP_Instance instance,
                            PP_Resource config_id,
                            PPB_Audio_Callback audio_callback,
                            void* user_data);

  // Initialization function for trusted init.
  bool Init(PP_Resource config_id,
            PPB_Audio_Callback user_callback,
            void* user_data);

  // Resource overrides.
  virtual ::ppapi::thunk::PPB_Audio_API* AsPPB_Audio_API();

  // PPB_Audio_API implementation.
  virtual PP_Resource GetCurrentConfig() OVERRIDE;
  virtual PP_Bool StartPlayback() OVERRIDE;
  virtual PP_Bool StopPlayback() OVERRIDE;
  virtual int32_t OpenTrusted(
      PP_Resource config_id,
      scoped_refptr< ::ppapi::TrackedCallback> create_callback) OVERRIDE;
  virtual int32_t GetSyncSocket(int* sync_socket) OVERRIDE;
  virtual int32_t GetSharedMemory(int* shm_handle, uint32_t* shm_size) OVERRIDE;

 private:
  // AudioHelper implementation.
  virtual void OnSetStreamInfo(base::SharedMemoryHandle shared_memory_handle,
                               size_t shared_memory_size_,
                               base::SyncSocket::Handle socket);

  // AudioConfig used for creating this Audio object. We own a ref.
  ::ppapi::ScopedPPResource config_;

  // PluginDelegate audio object that we delegate audio IPC through. We don't
  // own this pointer but are responsible for calling Shutdown on it.
  PluginDelegate::PlatformAudioOutput* audio_;

  // Track frame count for passing on to PPB_Audio_Shared::SetStreamInfo().
  int sample_frame_count_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Audio_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_AUDIO_IMPL_H_
