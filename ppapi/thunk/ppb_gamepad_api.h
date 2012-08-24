// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_GAMEPAD_API_H_
#define PPAPI_THUNK_PPB_GAMEPAD_API_H_

#include "ppapi/thunk/ppapi_thunk_export.h"

struct PP_GamepadsSampleData;

namespace ppapi {
namespace thunk {

class PPAPI_THUNK_EXPORT PPB_Gamepad_API {
 public:
  virtual ~PPB_Gamepad_API() {}

  virtual void Sample(PP_GamepadsSampleData* data) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_GAMEPAD_API_H_
