// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_SURFACE_3D_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_SURFACE_3D_IMPL_H_

#include "base/memory/ref_counted.h"
#include "base/task.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_surface_3d_api.h"
#include "webkit/plugins/ppapi/callbacks.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"

namespace webkit {
namespace ppapi {

class PPB_Context3D_Impl;

class PPB_Surface3D_Impl : public ::ppapi::Resource,
                           public ::ppapi::thunk::PPB_Surface3D_API {
 public:
  virtual ~PPB_Surface3D_Impl();

  static PP_Resource Create(PP_Instance instance,
                            PP_Config3D_Dev config,
                            const int32_t* attrib_list);

  // Resource override.
  virtual ::ppapi::thunk::PPB_Surface3D_API* AsPPB_Surface3D_API() OVERRIDE;

  // PPB_Surface3D_API implementation.
  virtual int32_t SetAttrib(int32_t attribute, int32_t value) OVERRIDE;
  virtual int32_t GetAttrib(int32_t attribute, int32_t* value) OVERRIDE;
  virtual int32_t SwapBuffers(PP_CompletionCallback callback) OVERRIDE;

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

  void ViewInitiatedPaint();
  void ViewFlushedPaint();
  void OnContextLost();

 private:
  explicit PPB_Surface3D_Impl(PP_Instance instance);

  bool Init(PP_Config3D_Dev config, const int32_t* attrib_list);

  // Called when SwapBuffers is complete.
  void OnSwapBuffers();
  void SendContextLost();

  bool bound_to_instance_;

  // True when the page's SwapBuffers has been issued but not returned yet.
  bool swap_initiated_;
  scoped_refptr<TrackedCompletionCallback> swap_callback_;

  // The context this surface is currently bound to.
  PPB_Context3D_Impl* context_;

  base::WeakPtrFactory<PPB_Surface3D_Impl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Surface3D_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_SURFACE_3D_IMPL_H_
