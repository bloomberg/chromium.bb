// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_LAYER_COMPOSITOR_API_H_
#define PPAPI_THUNK_PPB_LAYER_COMPOSITOR_API_H_

#include "base/memory/ref_counted.h"
#include "ppapi/c/dev/ppb_layer_compositor_dev.h"

namespace ppapi {

class TrackedCallback;

namespace thunk {

class PPB_LayerCompositor_API {
 public:
  virtual ~PPB_LayerCompositor_API() {}

  virtual PP_Bool AddLayer(PP_Resource layer) = 0;
  virtual void RemoveLayer(PP_Resource layer) = 0;
  virtual void SetZIndex(PP_Resource layer, int32_t index) = 0;
  virtual void SetRect(PP_Resource layer, const PP_Rect* rect) = 0;
  virtual void SetDisplay(PP_Resource layer, PP_Bool is_displayed) = 0;
  virtual void MarkAsDirty(PP_Resource layer) = 0;
  virtual int32_t SwapBuffers(scoped_refptr<TrackedCallback> callback) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_LAYER_COMPOSITOR_API_H_
