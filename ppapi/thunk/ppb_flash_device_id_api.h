// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

namespace ppapi {
namespace thunk {

class PPAPI_THUNK_EXPORT PPB_Flash_DeviceID_API {
 public:
  virtual ~PPB_Flash_DeviceID_API() {}

  virtual int32_t GetDeviceID(PP_Var* id,
                              const PP_CompletionCallback& callback) = 0;
};

}  // namespace thunk
}  // namespace ppapi

