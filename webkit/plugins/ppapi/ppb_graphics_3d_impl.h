// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_GRAPHICS_3D_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_GRAPHICS_3D_IMPL_H_

#include "ppapi/thunk/ppb_graphics_3d_api.h"
#include "webkit/plugins/ppapi/resource.h"

namespace webkit {
namespace ppapi {

class PPB_Graphics3D_Impl : public Resource,
                            public ::ppapi::thunk::PPB_Graphics3D_API {
 public:
  virtual ~PPB_Graphics3D_Impl();

  static PP_Resource Create(PluginInstance* instance,
                            PP_Config3D_Dev config,
                            PP_Resource share_context,
                            const int32_t* attrib_list);

  // ResourceObjectBase override.
  virtual ::ppapi::thunk::PPB_Graphics3D_API* AsPPB_Graphics3D_API() OVERRIDE;

  // PPB_Graphics3D_API implementation.
  virtual int32_t GetAttribs(int32_t* attrib_list) OVERRIDE;
  virtual int32_t SetAttribs(int32_t* attrib_list) OVERRIDE;
  virtual int32_t SwapBuffers(PP_CompletionCallback callback) OVERRIDE;

 private:
  explicit PPB_Graphics3D_Impl(PluginInstance* instance);

  bool Init(PP_Config3D_Dev config,
            PP_Resource share_context,
            const int32_t* attrib_list);

  DISALLOW_COPY_AND_ASSIGN(PPB_Graphics3D_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_GRAPHICS_3D_IMPL_H_

