// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_DEVICE_CONTEXT_2D_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_DEVICE_CONTEXT_2D_H_

#include <vector>

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

  bool Init(int width, int height, bool is_always_opaque);

  // Resource override.
  virtual DeviceContext2D* AsDeviceContext2D() { return this; }

  // PPB_DeviceContext2D functions.
  bool PaintImageData(PP_Resource image,
                      int32_t x, int32_t y,
                      const PP_Rect* src_rect);
  bool Scroll(const PP_Rect* clip_rect, int32_t dx, int32_t dy);
  bool ReplaceContents(PP_Resource image);
  bool Flush(PPB_DeviceContext2D_FlushCallback callback,
             void* callback_data);

  void Paint(WebKit::WebCanvas* canvas,
             const gfx::Rect& plugin_rect,
             const gfx::Rect& paint_rect);

 private:
  // Called internally to execute the different queued commands. The
  // parameters to these functions will have already been validated.
  void ExecutePaintImageData(const ImageData* image,
                             int x, int y,
                             const gfx::Rect& src_rect);
  void ExecuteScroll(const gfx::Rect& clip, int dx, int dy);
  void ExecuteReplaceContents(ImageData* image);

  scoped_refptr<ImageData> image_data_;

  // Keeps track of all drawing commands queued before a Flush call.
  struct QueuedOperation;
  typedef std::vector<QueuedOperation> OperationQueue;
  OperationQueue queued_operations_;

  DISALLOW_COPY_AND_ASSIGN(DeviceContext2D);
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_DEVICE_CONTEXT_2D_H_
