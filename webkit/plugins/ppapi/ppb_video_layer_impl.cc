// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_video_layer_impl.h"

#include "base/logging.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/ppb_video_layer_software.h"

using ppapi::thunk::PPB_VideoLayer_API;

namespace webkit {
namespace ppapi {

PPB_VideoLayer_Impl::PPB_VideoLayer_Impl(PP_Instance instance)
    : Resource(instance) {
}

PPB_VideoLayer_Impl::~PPB_VideoLayer_Impl() {
}

// static
PP_Resource PPB_VideoLayer_Impl::Create(PP_Instance instance,
                                        PP_VideoLayerMode_Dev mode) {
  if (mode != PP_VIDEOLAYERMODE_SOFTWARE)
    return 0;
  return (new PPB_VideoLayer_Software(instance))->GetReference();
}

PPB_VideoLayer_API* PPB_VideoLayer_Impl::AsPPB_VideoLayer_API() {
  return this;
}

}  // namespace ppapi
}  // namespace webkit
