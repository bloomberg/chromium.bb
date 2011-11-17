// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_AUDIO_HELPER_H_
#define WEBKIT_PLUGINS_PPAPI_AUDIO_HELPER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/shared_memory.h"
#include "base/sync_socket.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/scoped_pp_resource.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

namespace webkit {
namespace ppapi {

class AudioHelper : public PluginDelegate::PlatformAudioCommonClient {
 public:
  AudioHelper();
  virtual ~AudioHelper();

  // |PluginDelegate::PlatformAudioCommonClient| implementation.
  virtual void StreamCreated(base::SharedMemoryHandle shared_memory_handle,
                             size_t shared_memory_size_,
                             base::SyncSocket::Handle socket) OVERRIDE;

  void SetCallbackInfo(bool create_callback_pending,
                       PP_CompletionCallback create_callback);

 protected:
  // TODO(viettrungluu): This is all very poorly thought out. Refactor.

  // To be called by implementations of |PPB_Audio_API|/|PPB_AudioInput_API|.
  int32_t GetSyncSocketImpl(int* sync_socket);
  int32_t GetSharedMemoryImpl(int* shm_handle, uint32_t* shm_size);

  // To be implemented by subclasses to call their |SetStreamInfo()|.
  virtual void OnSetStreamInfo(base::SharedMemoryHandle shared_memory_handle,
                               size_t shared_memory_size,
                               base::SyncSocket::Handle socket_handle) = 0;

 private:
  // Is a create callback pending to fire?
  bool create_callback_pending_;

  // Trusted callback invoked from StreamCreated.
  PP_CompletionCallback create_callback_;

  // When a create callback is being issued, these will save the info for
  // querying from the callback. The proxy uses this to get the handles to the
  // other process instead of mapping them in the renderer. These will be
  // invalid all other times.
  scoped_ptr<base::SharedMemory> shared_memory_for_create_callback_;
  size_t shared_memory_size_for_create_callback_;
  scoped_ptr<base::SyncSocket> socket_for_create_callback_;

  DISALLOW_COPY_AND_ASSIGN(AudioHelper);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_AUDIO_HELPER_H_
