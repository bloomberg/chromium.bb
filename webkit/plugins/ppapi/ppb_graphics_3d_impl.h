// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_GRAPHICS_3D_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_GRAPHICS_3D_IMPL_H_

#include "ppapi/c/dev/ppb_graphics_3d_dev.h"
#include "webkit/plugins/ppapi/resource.h"

namespace webkit {
namespace ppapi {

class PPB_Graphics3D_Impl : public Resource {
 public:
  explicit PPB_Graphics3D_Impl(PluginInstance* instance);
  virtual ~PPB_Graphics3D_Impl();

  static const PPB_Graphics3D_Dev* GetInterface();

  // Resource override.
  virtual PPB_Graphics3D_Impl* AsPPB_Graphics3D_Impl();

  bool Init(PP_Config3D_Dev config,
            PP_Resource share_context,
            const int32_t* attrib_list);

 private:
  DISALLOW_COPY_AND_ASSIGN(PPB_Graphics3D_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_GRAPHICS_3D_IMPL_H_

