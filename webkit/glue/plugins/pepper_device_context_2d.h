// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_DEVICE_CONTEXT_2D_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_DEVICE_CONTEXT_2D_H_

#include "base/basictypes.h"
#include "third_party/ppapi/c/ppb_device_context_2d.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCanvas.h"
#include "webkit/glue/plugins/pepper_resource.h"

typedef struct _ppb_DeviceContext2D PPB_DeviceContext2D;

namespace gfx {
class Rect;
}

namespace pepper {

class ImageData;
class PluginModule;

class DeviceContext2D : public Resource {
 public:
  DeviceContext2D(PluginModule* module);
  virtual ~DeviceContext2D();

  // Returns a pointer to the interface implementing PPB_ImageData that is
  // exposed to the plugin.
  static const PPB_DeviceContext2D* GetInterface();

  bool Init(int width, int height);

  // Resource override.
  virtual DeviceContext2D* AsDeviceContext2D() { return this; }

  void PaintImageData(PP_Resource image,
                      int32_t x, int32_t y,
                      const PP_Rect* dirty,
                      uint32_t dirty_rect_count,
                      PPB_DeviceContext2D_PaintCallback callback,
                      void* callback_data);

  void Paint(WebKit::WebCanvas* canvas,
             const gfx::Rect& plugin_rect,
             const gfx::Rect& paint_rect);

 private:
  scoped_refptr<ImageData> image_data_;

  DISALLOW_COPY_AND_ASSIGN(DeviceContext2D);
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_DEVICE_CONTEXT_2D_H_
