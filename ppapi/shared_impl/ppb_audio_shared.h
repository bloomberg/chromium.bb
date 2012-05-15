// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPB_AUDIO_SHARED_H_
#define PPAPI_SHARED_IMPL_PPB_AUDIO_SHARED_H_

#include "base/memory/scoped_ptr.h"
#include "base/shared_memory.h"
#include "base/sync_socket.h"
#include "base/threading/simple_thread.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_audio_api.h"

namespace ppapi {

// Implements the logic to map shared memory and run the audio thread signaled
// from the sync socket. Both the proxy and the renderer implementation use
// this code.
class PPAPI_SHARED_EXPORT PPB_Audio_Shared
    : public thunk::PPB_Audio_API,
      public base::DelegateSimpleThread::Delegate {
 public:
  PPB_Audio_Shared();
  virtual ~PPB_Audio_Shared();

  // Keep in sync with media::AudioOutputController::kPauseMark.
  static const int kPauseMark;

  bool playing() const { return playing_; }

  // Sets the callback information that the background thread will use. This
  // is optional. Without a callback, the thread will not be run. This
  // non-callback mode is used in the renderer with the proxy, since the proxy
  // handles the callback entirely within the plugin process.
  void SetCallback(PPB_Audio_Callback callback, void* user_data);

  // Configures the current state to be playing or not. The caller is
  // responsible for ensuring the new state is the opposite of the current one.
  //
  // This is the implementation for PPB_Audio.Start/StopPlayback, except that
  // it does not actually notify the audio system to stop playback, it just
  // configures our object to stop generating callbacks. The actual stop
  // playback request will be done in the derived classes and will be different
  // from the proxy and the renderer.
  void SetStartPlaybackState();
  void SetStopPlaybackState();

  // Sets the shared memory and socket handles. This will automatically start
  // playback if we're currently set to play.
  void SetStreamInfo(PP_Instance instance,
                     base::SharedMemoryHandle shared_memory_handle,
                     size_t shared_memory_size,
                     base::SyncSocket::Handle socket_handle);

 private:
  // Starts execution of the audio thread.
  void StartThread();

  // Stop execution of the audio thread.
  void StopThread();

  // DelegateSimpleThread::Delegate implementation. Run on the audio thread.
  virtual void Run();

  // True if playing the stream.
  bool playing_;

  // Socket used to notify us when audio is ready to accept new samples. This
  // pointer is created in StreamCreated().
  scoped_ptr<base::CancelableSyncSocket> socket_;

  // Sample buffer in shared memory. This pointer is created in
  // StreamCreated(). The memory is only mapped when the audio thread is
  // created.
  scoped_ptr<base::SharedMemory> shared_memory_;

  // The size of the sample buffer in bytes.
  size_t shared_memory_size_;

  // When the callback is set, this thread is spawned for calling it.
  scoped_ptr<base::DelegateSimpleThread> audio_thread_;

  // Callback to call when audio is ready to accept new samples.
  PPB_Audio_Callback callback_;

  // User data pointer passed verbatim to the callback function.
  void* user_data_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Audio_Shared);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPB_AUDIO_SHARED_H_
