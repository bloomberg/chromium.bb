// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPB_AUDIO_INPUT_SHARED_H_
#define PPAPI_SHARED_IMPL_PPB_AUDIO_INPUT_SHARED_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/shared_memory.h"
#include "base/sync_socket.h"
#include "base/threading/simple_thread.h"
#include "ppapi/c/dev/ppb_audio_input_dev.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/scoped_pp_resource.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/ppb_audio_input_api.h"

namespace ppapi {

// Implements the logic to map shared memory and run the audio thread signaled
// from the sync socket. Both the proxy and the renderer implementation use
// this code.
class PPAPI_SHARED_EXPORT PPB_AudioInput_Shared
    : public Resource,
      public thunk::PPB_AudioInput_API,
      public base::DelegateSimpleThread::Delegate {
 public:
  // Used by the proxy.
  explicit PPB_AudioInput_Shared(const HostResource& audio_input);
  // Used by the impl.
  explicit PPB_AudioInput_Shared(PP_Instance instance);
  virtual ~PPB_AudioInput_Shared();

  // Resource overrides.
  virtual thunk::PPB_AudioInput_API* AsPPB_AudioInput_API() OVERRIDE;

  // Implementation of PPB_AudioInput_API non-trusted methods.
  virtual int32_t EnumerateDevices(
      PP_Resource* devices,
      const PP_CompletionCallback& callback) OVERRIDE;
  virtual int32_t Open(const std::string& device_id,
                       PP_Resource config,
                       PPB_AudioInput_Callback audio_input_callback,
                       void* user_data,
                       const PP_CompletionCallback& callback) OVERRIDE;
  virtual PP_Resource GetCurrentConfig() OVERRIDE;
  virtual PP_Bool StartCapture() OVERRIDE;
  virtual PP_Bool StopCapture() OVERRIDE;
  virtual void Close() OVERRIDE;

  void OnEnumerateDevicesComplete(int32_t result,
                                  const std::vector<DeviceRefData>& devices);
  void OnOpenComplete(int32_t result,
                      base::SharedMemoryHandle shared_memory_handle,
                      size_t shared_memory_size,
                      base::SyncSocket::Handle socket_handle);

  static PP_CompletionCallback MakeIgnoredCompletionCallback();

 protected:
  enum OpenState {
    BEFORE_OPEN,
    OPENED,
    CLOSED
  };

  // Subclasses should implement these methods to do impl- and proxy-specific
  // work.
  virtual int32_t InternalEnumerateDevices(PP_Resource* devices,
                                           PP_CompletionCallback callback) = 0;
  virtual int32_t InternalOpen(const std::string& device_id,
                               PP_AudioSampleRate sample_rate,
                               uint32_t sample_frame_count,
                               PP_CompletionCallback callback) = 0;
  virtual PP_Bool InternalStartCapture() = 0;
  virtual PP_Bool InternalStopCapture() = 0;
  virtual void InternalClose() = 0;

  // Configures the current state to be capturing or not. The caller is
  // responsible for ensuring the new state is the opposite of the current one.
  //
  // This is the implementation for PPB_AudioInput.Start/StopCapture, except
  // that it does not actually notify the audio system to stop capture, it just
  // configures our object to stop generating callbacks. The actual stop
  // capture request will be done in the derived classes and will be different
  // from the proxy and the renderer.
  void SetStartCaptureState();
  void SetStopCaptureState();

  // Sets the shared memory and socket handles. This will automatically start
  // capture if we're currently set to capture.
  void SetStreamInfo(base::SharedMemoryHandle shared_memory_handle,
                     size_t shared_memory_size,
                     base::SyncSocket::Handle socket_handle);

  // Starts execution of the audio input thread.
  void StartThread();

  // Stops execution of the audio input thread.
  void StopThread();

  // DelegateSimpleThread::Delegate implementation.
  // Run on the audio input thread.
  virtual void Run();

  // The common implementation of OpenTrusted() and Open(). It will call
  // InternalOpen() to do impl- and proxy-specific work.
  // OpenTrusted() will call this methods with a NULL |audio_input_callback|,
  // in this case, the thread will not be run. This non-callback mode is used in
  // the renderer with the proxy, since the proxy handles the callback entirely
  // within the plugin process.
  int32_t CommonOpen(const std::string& device_id,
                     PP_Resource config,
                     PPB_AudioInput_Callback audio_input_callback,
                     void* user_data,
                     PP_CompletionCallback callback);

  OpenState open_state_;

  // True if capturing the stream.
  bool capturing_;

  // Socket used to notify us when new samples are available. This pointer is
  // created in SetStreamInfo().
  scoped_ptr<base::CancelableSyncSocket> socket_;

  // Sample buffer in shared memory. This pointer is created in
  // SetStreamInfo(). The memory is only mapped when the audio thread is
  // created.
  scoped_ptr<base::SharedMemory> shared_memory_;

  // The size of the sample buffer in bytes.
  size_t shared_memory_size_;

  // When the callback is set, this thread is spawned for calling it.
  scoped_ptr<base::DelegateSimpleThread> audio_input_thread_;

  // Callback to call when new samples are available.
  PPB_AudioInput_Callback audio_input_callback_;

  // User data pointer passed verbatim to the callback function.
  void* user_data_;

  scoped_refptr<TrackedCallback> enumerate_devices_callback_;
  scoped_refptr<TrackedCallback> open_callback_;

  // Owning reference to the current config object. This isn't actually used,
  // we just dish it out as requested by the plugin.
  ScopedPPResource config_;

  // Output parameter of EnumerateDevices(). It should not be accessed after
  // |enumerate_devices_callback_| is run.
  PP_Resource* devices_;

  ResourceObjectType resource_object_type_;

  DISALLOW_COPY_AND_ASSIGN(PPB_AudioInput_Shared);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPB_AUDIO_INPUT_SHARED_H_
