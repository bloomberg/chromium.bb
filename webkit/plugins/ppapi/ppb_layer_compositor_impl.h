// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_LAYER_COMPOSITOR_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_LAYER_COMPOSITOR_IMPL_H_

#include "base/compiler_specific.h"
#include "ppapi/thunk/ppb_layer_compositor_api.h"
#include "ppapi/shared_impl/resource.h"

struct PP_Rect;

namespace webkit {
namespace ppapi {

class PPB_LayerCompositor_Impl
    : public ::ppapi::Resource,
      public ::ppapi::thunk::PPB_LayerCompositor_API {
 public:
  explicit PPB_LayerCompositor_Impl(PP_Instance instance);
  virtual ~PPB_LayerCompositor_Impl();

  static const PPB_LayerCompositor_Dev* GetInterface();

  // Resource override.
  virtual PPB_LayerCompositor_API* AsPPB_LayerCompositor_API();

  // PPB_LayerCompositor_API implementation.
  virtual PP_Bool AddLayer(PP_Resource layer) OVERRIDE;
  virtual void RemoveLayer(PP_Resource layer) OVERRIDE;
  virtual void SetZIndex(PP_Resource layer, int32_t index) OVERRIDE;
  virtual void SetRect(PP_Resource layer, const PP_Rect* rect) OVERRIDE;
  virtual void SetDisplay(PP_Resource layer, PP_Bool is_displayed) OVERRIDE;
  virtual void MarkAsDirty(PP_Resource layer) OVERRIDE;
  virtual int32_t SwapBuffers(PP_CompletionCallback callback) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(PPB_LayerCompositor_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_LAYER_COMPOSITOR_IMPL_H_
