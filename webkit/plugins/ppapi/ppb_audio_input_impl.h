// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_AUDIO_INPUT_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_AUDIO_INPUT_IMPL_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "ppapi/shared_impl/ppb_audio_input_shared.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"

namespace webkit {
namespace ppapi {

// Some of the backend functionality of this class is implemented by the
// PPB_AudioInput_Shared so it can be shared with the proxy.
class PPB_AudioInput_Impl : public ::ppapi::PPB_AudioInput_Shared,
                            public PluginDelegate::PlatformAudioInputClient,
                            public base::SupportsWeakPtr<PPB_AudioInput_Impl> {
 public:
  typedef std::vector< ::ppapi::DeviceRefData> DeviceRefDataVector;

  explicit PPB_AudioInput_Impl(PP_Instance instance);
  virtual ~PPB_AudioInput_Impl();

  // Creation function for the v0.1 interface. This handles all initialization
  // and will return 0 on failure.
  static PP_Resource Create0_1(PP_Instance instance,
                               PP_Resource config,
                               PPB_AudioInput_Callback audio_input_callback,
                               void* user_data);

  // Implementation of PPB_AudioInput_API trusted methods.
  virtual int32_t OpenTrusted(
      const std::string& device_id,
      PP_Resource config,
      scoped_refptr< ::ppapi::TrackedCallback> create_callback) OVERRIDE;
  virtual int32_t GetSyncSocket(int* sync_socket) OVERRIDE;
  virtual int32_t GetSharedMemory(int* shm_handle, uint32_t* shm_size) OVERRIDE;
  virtual const DeviceRefDataVector& GetDeviceRefData() const OVERRIDE;

  // PluginDelegate::PlatformAudioInputClient implementation.
  virtual void StreamCreated(base::SharedMemoryHandle shared_memory_handle,
                             size_t shared_memory_size,
                             base::SyncSocket::Handle socket) OVERRIDE;
  virtual void StreamCreationFailed() OVERRIDE;

 private:
  // PPB_AudioInput_Shared implementation.
  virtual int32_t InternalEnumerateDevices(
      PP_Resource* devices,
      scoped_refptr< ::ppapi::TrackedCallback> callback) OVERRIDE;
  virtual int32_t InternalOpen(
      const std::string& device_id,
      PP_AudioSampleRate sample_rate,
      uint32_t sample_frame_count,
      scoped_refptr< ::ppapi::TrackedCallback> callback) OVERRIDE;
  virtual PP_Bool InternalStartCapture() OVERRIDE;
  virtual PP_Bool InternalStopCapture() OVERRIDE;
  virtual void InternalClose() OVERRIDE;

  void EnumerateDevicesCallbackFunc(int request_id,
                                    bool succeeded,
                                    const DeviceRefDataVector& devices);

  // PluginDelegate audio input object that we delegate audio IPC through.
  // We don't own this pointer but are responsible for calling Shutdown on it.
  PluginDelegate::PlatformAudioInput* audio_input_;

  DeviceRefDataVector devices_data_;

  DISALLOW_COPY_AND_ASSIGN(PPB_AudioInput_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_AUDIO_INPUT_IMPL_H_
