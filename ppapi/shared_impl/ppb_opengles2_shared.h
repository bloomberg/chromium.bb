// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPB_OPENGLES2_SHARED_H_
#define PPAPI_SHARED_IMPL_PPB_OPENGLES2_SHARED_H_

#include "ppapi/c/ppb_opengles2.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

class PPAPI_SHARED_EXPORT PPB_OpenGLES2_Shared {
 public:
  static const PPB_OpenGLES2* GetInterface();
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPB_OPENGLES2_SHARED_H_

