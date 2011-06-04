// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_LAYER_COMPOSITOR_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_LAYER_COMPOSITOR_IMPL_H_

#include "ppapi/c/dev/ppb_layer_compositor_dev.h"
#include "webkit/plugins/ppapi/resource.h"

struct PP_Rect;
struct PP_Size;

namespace webkit {
namespace ppapi {

class PluginInstance;

class PPB_LayerCompositor_Impl : public Resource {
 public:
  explicit PPB_LayerCompositor_Impl(PluginInstance* instance);
  virtual ~PPB_LayerCompositor_Impl();

  static const PPB_LayerCompositor_Dev* GetInterface();

  // Resource override.
  virtual PPB_LayerCompositor_Impl* AsPPB_LayerCompositor_Impl();

  PP_Bool AddLayer(PP_Resource layer);
  void RemoveLayer(PP_Resource layer);
  void SetZIndex(PP_Resource layer, int32_t index);
  void SetRect(PP_Resource layer, const struct PP_Rect* rect);
  void SetDisplay(PP_Resource layer, PP_Bool is_displayed);
  void MarkAsDirty(PP_Resource layer);
  int32_t SwapBuffers(struct PP_CompletionCallback callback);

 private:
  DISALLOW_COPY_AND_ASSIGN(PPB_LayerCompositor_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_LAYER_COMPOSITOR_IMPL_H_
