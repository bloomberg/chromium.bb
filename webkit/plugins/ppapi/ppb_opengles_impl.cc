// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_opengles_impl.h"

#include "ppapi/shared_impl/opengles2_impl.h"

namespace webkit {
namespace ppapi {

const PPB_OpenGLES2_Dev* PPB_OpenGLES_Impl::GetInterface() {
  return ::ppapi::OpenGLES2Impl::GetInterface();
}

}  // namespace ppapi
}  // namespace webkit

