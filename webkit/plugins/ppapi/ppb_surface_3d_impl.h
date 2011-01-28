// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_SURFACE_3D_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_SURFACE_3D_IMPL_H_

#include "base/callback.h"
#include "ppapi/c/dev/ppb_surface_3d_dev.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/resource.h"

namespace gfx {
class Size;
}

namespace webkit {
namespace ppapi {

class PPB_Surface3D_Impl : public Resource {
 public:
  explicit PPB_Surface3D_Impl(PluginInstance* instance);
  virtual ~PPB_Surface3D_Impl();

  static const PPB_Surface3D_Dev* GetInterface();

  // Resource override.
  virtual PPB_Surface3D_Impl* AsPPB_Surface3D_Impl();

  bool Init(PP_Config3D_Dev config,
            const int32_t* attrib_list);

  PPB_Context3D_Impl* context() const {
    return context_;
  }

  // Binds/unbinds the graphics of this surface with the associated instance.
  // If the surface is bound, anything drawn on the surface appears on instance
  // window. Returns true if binding/unbinding is successful.
  bool BindToInstance(bool bind);

  // Binds the context such that all draw calls to context
  // affect this surface. To unbind call this function will NULL context.
  // Returns true if successful.
  bool BindToContext(PPB_Context3D_Impl* context);

  unsigned int GetBackingTextureId();

  int32_t SwapBuffers(PP_CompletionCallback callback);

  void ViewInitiatedPaint();
  void ViewFlushedPaint();

 private:
  // Called when SwapBuffers is complete.
  void OnSwapBuffers();

  bool bound_to_instance_;

  // True when the page's SwapBuffers has been issued but not returned yet.
  bool swap_initiated_;
  PP_CompletionCallback swap_callback_;

  // The context this surface is currently bound to.
  PPB_Context3D_Impl* context_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Surface3D_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_SURFACE_3D_IMPL_H_
