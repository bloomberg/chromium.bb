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

// Converts a rect inside an image of the given dimensions. The rect may be
// NULL to indicate it should be the entire image. If the rect is outside of
// the image, this will do nothing and return false.
bool ValidateAndConvertRect(const PP_Rect* rect,
                            int image_width, int image_height,
                            gfx::Rect* dest) {
  if (!rect) {
    // Use the entire image area.
    *dest = gfx::Rect(0, 0, image_width, image_height);
  } else {
    // Validate the passed-in area.
    if (rect->point.x < 0 || rect->point.y < 0 ||
        rect->size.width <= 0 || rect->size.height <= 0)
      return false;

    // Check the max bounds, being careful of overflow.
    if (static_cast<int64>(rect->point.x) +
        static_cast<int64>(rect->size.width) >
        static_cast<int64>(image_width))
      return false;
    if (static_cast<int64>(rect->point.y) +
        static_cast<int64>(rect->size.height) >
        static_cast<int64>(image_height))
      return false;

    *dest = gfx::Rect(rect->point.x, rect->point.y,
                      rect->size.width, rect->size.height);
  }
  return true;
}

PP_Resource Create(PP_Module module_id, int32_t width, int32_t height,
                   bool is_always_opaque) {
  PluginModule* module = PluginModule::FromPPModule(module_id);
  if (!module)
    return NullPPResource();

  scoped_refptr<DeviceContext2D> context(new DeviceContext2D(module));
  if (!context->Init(width, height, is_always_opaque))
    return NullPPResource();
  context->AddRef();  // AddRef for the caller.
  return context->GetResource();
}

bool PaintImageData(PP_Resource device_context,
                    PP_Resource image,
                    int32_t x, int32_t y,
                    const PP_Rect* src_rect) {
  scoped_refptr<DeviceContext2D> context(
      ResourceTracker::Get()->GetAsDeviceContext2D(device_context));
  if (!context.get())
    return false;
  return context->PaintImageData(image, x, y, src_rect);
}

bool Scroll(PP_Resource device_context,
            const PP_Rect* clip_rect,
            int32_t dx, int32_t dy) {
  scoped_refptr<DeviceContext2D> context(
      ResourceTracker::Get()->GetAsDeviceContext2D(device_context));
  if (!context.get())
    return false;
  return context->Scroll(clip_rect, dx, dy);
}

bool ReplaceContents(PP_Resource device_context, PP_Resource image) {
  scoped_refptr<DeviceContext2D> context(
      ResourceTracker::Get()->GetAsDeviceContext2D(device_context));
  if (!context.get())
    return false;
  return context->ReplaceContents(image);
}

bool Flush(PP_Resource device_context,
           PPB_DeviceContext2D_FlushCallback callback,
           void* callback_data) {
  scoped_refptr<DeviceContext2D> context(
      ResourceTracker::Get()->GetAsDeviceContext2D(device_context));
  if (!context.get())
    return false;
  return context->Flush(callback, callback_data);
}

const PPB_DeviceContext2D ppb_devicecontext2d = {
  &Create,
  &PaintImageData,
  &Scroll,
  &ReplaceContents,
  &Flush
};

}  // namespace

struct DeviceContext2D::QueuedOperation {
  enum Type {
    PAINT,
    SCROLL,
    REPLACE
  };

  QueuedOperation(Type t)
      : type(t),
        paint_x(0),
        paint_y(0),
        scroll_dx(0),
        scroll_dy(0) {
  }

  Type type;

  // Valid when type == PAINT.
  scoped_refptr<ImageData> paint_image;
  int paint_x, paint_y;
  gfx::Rect paint_src_rect;

  // Valid when type == SCROLL.
  gfx::Rect scroll_clip_rect;
  int scroll_dx, scroll_dy;

  // Valid when type == REPLACE.
  scoped_refptr<ImageData> replace_image;
};

DeviceContext2D::DeviceContext2D(PluginModule* module) : Resource(module) {
}

DeviceContext2D::~DeviceContext2D() {
}

// static
const PPB_DeviceContext2D* DeviceContext2D::GetInterface() {
  return &ppb_devicecontext2d;
}

bool DeviceContext2D::Init(int width, int height, bool is_always_opaque) {
  image_data_ = new ImageData(module());
  if (!image_data_->Init(PP_IMAGEDATAFORMAT_BGRA_PREMUL, width, height, true) ||
      !image_data_->Map()) {
    image_data_ = NULL;
    return false;
  }

  return true;
}

bool DeviceContext2D::PaintImageData(PP_Resource image,
                                     int32_t x, int32_t y,
                                     const PP_Rect* src_rect) {
  scoped_refptr<ImageData> image_resource(
      ResourceTracker::Get()->GetAsImageData(image));
  if (!image_resource.get() || !image_resource->is_valid())
    return false;

  const SkBitmap& new_image_bitmap = image_resource->GetMappedBitmap();

  QueuedOperation operation(QueuedOperation::PAINT);
  operation.paint_image = image_resource;
  if (!ValidateAndConvertRect(src_rect, new_image_bitmap.width(),
                              new_image_bitmap.height(),
                              &operation.paint_src_rect))
    return false;

  // Validate the bitmap position using the previously-validated rect.
  if (x < 0 ||
      static_cast<int64>(x) +
      static_cast<int64>(operation.paint_src_rect.right()) >
      image_data_->width())
    return false;
  if (y < 0 ||
      static_cast<int64>(y) +
      static_cast<int64>(operation.paint_src_rect.bottom()) >
      image_data_->height())
    return false;
  operation.paint_x = x;
  operation.paint_y = y;

  queued_operations_.push_back(operation);
  return true;
}

bool DeviceContext2D::Scroll(const PP_Rect* clip_rect,
                             int32_t dx, int32_t dy) {
  QueuedOperation operation(QueuedOperation::SCROLL);
  if (!ValidateAndConvertRect(clip_rect,
                              image_data_->width(),
                              image_data_->height(),
                              &operation.scroll_clip_rect))
    return false;

  // If we're being asked to scroll by more than the clip rect size, just
  // ignore this scroll command and say it worked.
  if (dx <= -image_data_->width() || dx >= image_data_->width() ||
      dx <= -image_data_->height() || dy >= image_data_->height())
    return true;

  operation.scroll_dx = dx;
  operation.scroll_dy = dy;

  queued_operations_.push_back(operation);
  return false;
}

bool DeviceContext2D::ReplaceContents(PP_Resource image) {
  scoped_refptr<ImageData> image_resource(
      ResourceTracker::Get()->GetAsImageData(image));
  if (!image_resource.get() || !image_resource->is_valid())
    return false;

  if (image_resource->width() != image_data_->width() ||
      image_resource->height() != image_data_->height())
    return false;

  QueuedOperation operation(QueuedOperation::REPLACE);
  operation.replace_image = new ImageData(image_resource->module());
  queued_operations_.push_back(operation);

  // Swap the input image data with the new one we just made in the
  // QueuedOperation. This way the plugin still gets to manage the reference
  // count of the old object without having memory released out from under it.
  // But it ensures that after this, if the plugin does try to use the image
  // it gave us, those operations will fail.
  operation.replace_image->Swap(image_resource.get());

  return false;
}

bool DeviceContext2D::Flush(PPB_DeviceContext2D_FlushCallback callback,
                            void* callback_data) {
  for (size_t i = 0; i < queued_operations_.size(); i++) {
    QueuedOperation& operation = queued_operations_[i];
    switch (operation.type) {
      case QueuedOperation::PAINT:
        ExecutePaintImageData(operation.paint_image.get(),
                              operation.paint_x, operation.paint_y,
                              operation.paint_src_rect);
        break;
      case QueuedOperation::SCROLL:
        ExecuteScroll(operation.scroll_clip_rect,
                      operation.scroll_dx, operation.scroll_dy);
        break;
      case QueuedOperation::REPLACE:
        ExecuteReplaceContents(operation.replace_image.get());
        break;
    }
  }
  queued_operations_.clear();
 // TODO(brettw) implement invalidate and callbacks!

  // Cause the updated part of the screen to be repainted. This will happen
  // asynchronously.
  /*
  gfx::Rect dest_gfx_rect(src_rect->left, src_rect->top,
                          src_rect->right - src_rect->left,
                          src_rect->bottom - src_rect->top);

  plugin_delegate_->instance()->webplugin()->InvalidateRect(dest_gfx_rect);

  // Save the callback to execute later. See |unpainted_flush_callbacks_| in
  // the header file.
  if (callback) {
    unpainted_flush_callbacks_.push_back(
        FlushCallbackData(callback, id, context, user_data));
  }
*/
  return true;
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


void DeviceContext2D::ExecutePaintImageData(const ImageData* image,
                                            int x, int y,
                                            const gfx::Rect& src_rect) {
  // Portion within the source image to cut out.
  SkIRect src_irect = { src_rect.x(), src_rect.y(),
                        src_rect.right(), src_rect.bottom() };

  // Location within the backing store to copy to.
  SkRect dest_rect = { SkIntToScalar(x + src_rect.x()),
                       SkIntToScalar(y + src_rect.y()),
                       SkIntToScalar(x + src_rect.right()),
                       SkIntToScalar(y + src_rect.bottom()) };

  // We're guaranteed to have a mapped canvas since we mapped it in Init().
  skia::PlatformCanvas* backing_canvas = image_data_->mapped_canvas();

  // We want to replace the contents of the bitmap rather than blend.
  SkPaint paint;
  paint.setXfermodeMode(SkXfermode::kSrc_Mode);
  backing_canvas->drawBitmapRect(image->GetMappedBitmap(),
                                 &src_irect, dest_rect, &paint);
}

void DeviceContext2D::ExecuteScroll(const gfx::Rect& clip, int dx, int dy) {
  // FIXME(brettw)
}

void DeviceContext2D::ExecuteReplaceContents(ImageData* image) {
  image_data_->Swap(image);
}

}  // namespace pepper
