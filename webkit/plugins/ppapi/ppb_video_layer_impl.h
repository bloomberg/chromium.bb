// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_VIDEO_LAYER_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_VIDEO_LAYER_IMPL_H_

#include "ppapi/thunk/ppb_video_layer_api.h"
#include "webkit/plugins/ppapi/resource.h"

struct PP_Rect;
struct PP_Size;

namespace webkit {
namespace ppapi {

class PluginInstance;

class PPB_VideoLayer_Impl : public Resource,
                            public ::ppapi::thunk::PPB_VideoLayer_API {
 public:
  virtual ~PPB_VideoLayer_Impl();

  static PP_Resource Create(PluginInstance* instance,
                            PP_VideoLayerMode_Dev mode);

  // Resource override.
  virtual PPB_VideoLayer_API* AsPPB_VideoLayer_API() OVERRIDE;

  // Derived classes must implement PPB_VideoLayer_API.

 protected:
  explicit PPB_VideoLayer_Impl(PluginInstance* instance);

 private:
  DISALLOW_COPY_AND_ASSIGN(PPB_VideoLayer_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_VIDEO_LAYER_IMPL_H_
