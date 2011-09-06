// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_OPENGLES2_IMPL_H_
#define PPAPI_SHARED_IMPL_OPENGLES2_IMPL_H_

#include "ppapi/c/dev/ppb_opengles_dev.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

class PPAPI_SHARED_EXPORT OpenGLES2Impl {
 public:
  static const PPB_OpenGLES2_Dev* GetInterface();
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_OPENGLES2_IMPL_H_

