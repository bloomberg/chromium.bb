// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPB_OPENGLES2_SHARED_H_
#define PPAPI_SHARED_IMPL_PPB_OPENGLES2_SHARED_H_

#include "ppapi/c/ppb_opengles2.h"
#include "ppapi/c/dev/ppb_opengles2ext_dev.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

class PPAPI_SHARED_EXPORT PPB_OpenGLES2_Shared {
 public:
  static const PPB_OpenGLES2* GetInterface();
  static const PPB_OpenGLES2InstancedArrays_Dev* GetInstancedArraysInterface();
  static const PPB_OpenGLES2FramebufferBlit_Dev* GetFramebufferBlitInterface();
  static const PPB_OpenGLES2FramebufferMultisample_Dev*
      GetFramebufferMultisampleInterface();
  static const PPB_OpenGLES2ChromiumEnableFeature_Dev*
      GetChromiumEnableFeatureInterface();
  static const PPB_OpenGLES2ChromiumMapSub_Dev* GetChromiumMapSubInterface();
  static const PPB_OpenGLES2Query_Dev* GetQueryInterface();
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPB_OPENGLES2_SHARED_H_

