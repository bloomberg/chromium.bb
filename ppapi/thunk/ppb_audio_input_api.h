// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_AUDIO_INPUT_API_H_
#define PPAPI_THUNK_AUDIO_INPUT_API_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "ppapi/c/dev/ppb_audio_input_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {

struct DeviceRefData;
class TrackedCallback;

namespace thunk {

class PPAPI_THUNK_EXPORT PPB_AudioInput_API {
 public:
  virtual ~PPB_AudioInput_API() {}

  virtual int32_t EnumerateDevices(PP_Resource* devices,
                                   scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t Open(const std::string& device_id,
                       PP_Resource config,
                       PPB_AudioInput_Callback audio_input_callback,
                       void* user_data,
                       scoped_refptr<TrackedCallback> callback) = 0;
  virtual PP_Resource GetCurrentConfig() = 0;
  virtual PP_Bool StartCapture() = 0;
  virtual PP_Bool StopCapture() = 0;
  virtual void Close() = 0;

  // Trusted API.
  virtual int32_t OpenTrusted(
      const std::string& device_id,
      PP_Resource config,
      scoped_refptr<TrackedCallback> create_callback) = 0;
  virtual int32_t GetSyncSocket(int* sync_socket) = 0;
  virtual int32_t GetSharedMemory(int* shm_handle, uint32_t* shm_size) = 0;
  virtual const std::vector<DeviceRefData>& GetDeviceRefData() const = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_AUDIO_INPUT_API_H_
