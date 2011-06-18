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

PPB_VideoLayer_Impl::PPB_VideoLayer_Impl(PluginInstance* instance)
    : Resource(instance) {
}

PPB_VideoLayer_Impl::~PPB_VideoLayer_Impl() {
}

// static
PP_Resource PPB_VideoLayer_Impl::Create(PP_Instance pp_instance,
                                        PP_VideoLayerMode_Dev mode) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(pp_instance);
  if (!instance)
    return 0;

  if (mode != PP_VIDEOLAYERMODE_SOFTWARE)
    return 0;

  scoped_refptr<PPB_VideoLayer_Impl> layer(
      new PPB_VideoLayer_Software(instance));
  return layer->GetReference();
}

PPB_VideoLayer_API* PPB_VideoLayer_Impl::AsPPB_VideoLayer_API() {
  return this;
}

}  // namespace ppapi
}  // namespace webkit
