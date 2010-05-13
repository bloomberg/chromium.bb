// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_device_context_2d.h"

#include "base/logging.h"
#include "gfx/point.h"
#include "gfx/rect.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/ppapi/c/pp_module.h"
#include "third_party/ppapi/c/pp_rect.h"
#include "third_party/ppapi/c/pp_resource.h"
#include "third_party/ppapi/c/ppb_device_context_2d.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "webkit/glue/plugins/pepper_image_data.h"
#include "webkit/glue/plugins/pepper_plugin_module.h"
#include "webkit/glue/plugins/pepper_resource_tracker.h"

#if defined(OS_MACOSX)
#include "base/mac_util.h"
#include "base/scoped_cftyperef.h"
#endif

namespace pepper {

namespace {

PP_Resource Create(PP_Module module_id, int32_t width, int32_t height) {
  PluginModule* module = PluginModule::FromPPModule(module_id);
  if (!module)
    return NullPPResource();

  scoped_refptr<DeviceContext2D> context(new DeviceContext2D(module));
  if (!context->Init(width, height))
    return NullPPResource();
  context->AddRef();  // AddRef for the caller.
  return context->GetResource();
}

void PaintImageData(PP_Resource device_context,
                    PP_Resource image,
                    int32_t x, int32_t y,
                    const PP_Rect* dirty,
                    uint32_t dirty_rect_count,
                    PPB_DeviceContext2D_PaintCallback callback,
                    void* callback_data) {
  scoped_refptr<Resource> device_resource =
      ResourceTracker::Get()->GetResource(device_context);
  if (!device_resource.get())
    return;
  DeviceContext2D* context = device_resource->AsDeviceContext2D();
  if (!context)
    return;
  context->PaintImageData(image, x, y, dirty, dirty_rect_count,
                          callback, callback_data);
}

const PPB_DeviceContext2D ppb_devicecontext2d = {
  &Create,
  &PaintImageData,
};

}  // namespace

DeviceContext2D::DeviceContext2D(PluginModule* module) : Resource(module) {
}

DeviceContext2D::~DeviceContext2D() {
}

// static
const PPB_DeviceContext2D* DeviceContext2D::GetInterface() {
  return &ppb_devicecontext2d;
}

bool DeviceContext2D::Init(int width, int height) {
  image_data_ = new ImageData(module());
  if (!image_data_->Init(PP_IMAGEDATAFORMAT_BGRA_PREMUL, width, height) ||
      !image_data_->Map()) {
    image_data_ = NULL;
    return false;
  }

  return true;
}

void DeviceContext2D::PaintImageData(PP_Resource image,
                                     int32_t x, int32_t y,
                                     const PP_Rect* dirty,
                                     uint32_t dirty_rect_count,
                                     PPB_DeviceContext2D_PaintCallback callback,
                                     void* callback_data) {
  scoped_refptr<Resource> image_resource =
      ResourceTracker::Get()->GetResource(image);
  if (!image_resource.get())
    return;
  ImageData* new_image_data = image_resource->AsImageData();
  if (!new_image_data)
    return;

  const SkBitmap& new_image_bitmap = new_image_data->GetMappedBitmap();

  // TODO(brettw) handle multiple dirty rects.
  DCHECK(dirty_rect_count <= 1);

  // Draw the bitmap to the backing store.
  SkIRect src_rect;
  if (dirty_rect_count == 0 || !dirty) {
    // Default to the entire bitmap.
    src_rect.fLeft = 0;
    src_rect.fTop = 0;
    src_rect.fRight = new_image_bitmap.width();
    src_rect.fBottom = new_image_bitmap.height();
  } else {
    src_rect.fLeft = dirty->point.x;
    src_rect.fTop = dirty->point.y;
    src_rect.fRight = dirty->point.x + dirty->size.width;
    src_rect.fBottom = dirty->point.y + dirty->size.height;
  }
  SkRect dest_rect = { SkIntToScalar(src_rect.fLeft),
                       SkIntToScalar(src_rect.fTop),
                       SkIntToScalar(src_rect.fRight),
                       SkIntToScalar(src_rect.fBottom) };

  // We're guaranteed to have a mapped canvas since we mapped it in Init().
  skia::PlatformCanvas* backing_canvas = image_data_->mapped_canvas();

  // We want to replace the contents of the bitmap rather than blend.
  SkPaint paint;
  paint.setXfermodeMode(SkXfermode::kSrc_Mode);
  backing_canvas->drawBitmapRect(new_image_bitmap,
                                 &src_rect, dest_rect, &paint);

  // TODO(brettw) implement invalidate and callbacks!

  // Cause the updated part of the screen to be repainted. This will happen
  // asynchronously.
  /*
  gfx::Rect dest_gfx_rect(dirty->left, dirty->top,
                          dirty->right - dirty->left,
                          dirty->bottom - dirty->top);

  plugin_delegate_->instance()->webplugin()->InvalidateRect(dest_gfx_rect);

  // Save the callback to execute later. See |unpainted_flush_callbacks_| in
  // the header file.
  if (callback) {
    unpainted_flush_callbacks_.push_back(
        FlushCallbackData(callback, id, context, user_data));
  }
*/
}

void DeviceContext2D::Paint(WebKit::WebCanvas* canvas,
                            const gfx::Rect& plugin_rect,
                            const gfx::Rect& paint_rect) {
  // We're guaranteed to have a mapped canvas since we mapped it in Init().
  const SkBitmap& backing_bitmap = image_data_->GetMappedBitmap();

#if defined(OS_MACOSX)
  SkAutoLockPixels lock(backing_bitmap);

  scoped_cftyperef<CGDataProviderRef> data_provider(
      CGDataProviderCreateWithData(
          NULL, backing_bitmap.getAddr32(0, 0),
          backing_bitmap.rowBytes() * backing_bitmap.height(), NULL));
  scoped_cftyperef<CGImageRef> image(
      CGImageCreate(
          backing_bitmap.width(), backing_bitmap.height(),
          8, 32, backing_bitmap.rowBytes(),
          mac_util::GetSystemColorSpace(),
          kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host,
          data_provider, NULL, false, kCGRenderingIntentDefault));

  // Flip the transform
  CGContextSaveGState(canvas);
  float window_height = static_cast<float>(CGBitmapContextGetHeight(canvas));
  CGContextTranslateCTM(canvas, 0, window_height);
  CGContextScaleCTM(canvas, 1.0, -1.0);

  CGRect bounds;
  bounds.origin.x = plugin_rect.origin().x();
  bounds.origin.y = window_height - plugin_rect.origin().y() -
      backing_bitmap.height();
  bounds.size.width = backing_bitmap.width();
  bounds.size.height = backing_bitmap.height();

  CGContextDrawImage(canvas, bounds, image);
  CGContextRestoreGState(canvas);
#else
  gfx::Point origin(plugin_rect.origin().x(), plugin_rect.origin().y());
  canvas->drawBitmap(backing_bitmap,
                     SkIntToScalar(plugin_rect.origin().x()),
                     SkIntToScalar(plugin_rect.origin().y()));
#endif
}

}  // namespace pepper
