// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_graphics_2d.h"

#include <iterator>

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "gfx/blit.h"
#include "gfx/point.h"
#include "gfx/rect.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/ppapi/c/pp_errors.h"
#include "third_party/ppapi/c/pp_module.h"
#include "third_party/ppapi/c/pp_rect.h"
#include "third_party/ppapi/c/pp_resource.h"
#include "third_party/ppapi/c/ppb_graphics_2d.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "webkit/glue/plugins/pepper_image_data.h"
#include "webkit/glue/plugins/pepper_plugin_instance.h"
#include "webkit/glue/plugins/pepper_plugin_module.h"

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

PP_Resource Create(PP_Module module_id,
                   const PP_Size* size,
                   bool is_always_opaque) {
  PluginModule* module = PluginModule::FromPPModule(module_id);
  if (!module)
    return 0;

  scoped_refptr<Graphics2D> context(new Graphics2D(module));
  if (!context->Init(size->width, size->height, is_always_opaque))
    return 0;
  return context->GetReference();
}

bool IsGraphics2D(PP_Resource resource) {
  return !!Resource::GetAs<Graphics2D>(resource);
}

bool Describe(PP_Resource device_context,
              PP_Size* size,
              bool* is_always_opaque) {
  scoped_refptr<Graphics2D> context(
      Resource::GetAs<Graphics2D>(device_context));
  if (!context)
    return false;
  return context->Describe(size, is_always_opaque);
}

bool PaintImageData(PP_Resource device_context,
                    PP_Resource image,
                    const PP_Point* top_left,
                    const PP_Rect* src_rect) {
  scoped_refptr<Graphics2D> context(
      Resource::GetAs<Graphics2D>(device_context));
  if (!context)
    return false;
  return context->PaintImageData(image, top_left, src_rect);
}

bool Scroll(PP_Resource device_context,
            const PP_Rect* clip_rect,
            const PP_Point* amount) {
  scoped_refptr<Graphics2D> context(
      Resource::GetAs<Graphics2D>(device_context));
  if (!context)
    return false;
  return context->Scroll(clip_rect, amount);
}

bool ReplaceContents(PP_Resource device_context, PP_Resource image) {
  scoped_refptr<Graphics2D> context(
      Resource::GetAs<Graphics2D>(device_context));
  if (!context)
    return false;
  return context->ReplaceContents(image);
}

int32_t Flush(PP_Resource device_context,
              PP_CompletionCallback callback) {
  scoped_refptr<Graphics2D> context(
      Resource::GetAs<Graphics2D>(device_context));
  if (!context)
    return PP_ERROR_BADRESOURCE;
  return context->Flush(callback);
}

const PPB_Graphics2D ppb_graphics_2d = {
  &Create,
  &IsGraphics2D,
  &Describe,
  &PaintImageData,
  &Scroll,
  &ReplaceContents,
  &Flush
};

}  // namespace

struct Graphics2D::QueuedOperation {
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

Graphics2D::Graphics2D(PluginModule* module)
    : Resource(module),
      bound_instance_(NULL),
      flushed_any_data_(false),
      offscreen_flush_pending_(false),
      is_always_opaque_(false) {
}

Graphics2D::~Graphics2D() {
}

// static
const PPB_Graphics2D* Graphics2D::GetInterface() {
  return &ppb_graphics_2d;
}

bool Graphics2D::Init(int width, int height, bool is_always_opaque) {
  // The underlying ImageData will validate the dimensions.
  image_data_ = new ImageData(module());
  if (!image_data_->Init(PP_IMAGEDATAFORMAT_BGRA_PREMUL, width, height, true) ||
      !image_data_->Map()) {
    image_data_ = NULL;
    return false;
  }
  is_always_opaque_ = is_always_opaque;
  return true;
}

bool Graphics2D::Describe(PP_Size* size, bool* is_always_opaque) {
  size->width = image_data_->width();
  size->height = image_data_->height();
  *is_always_opaque = false;  // TODO(brettw) implement this.
  return true;
}

bool Graphics2D::PaintImageData(PP_Resource image,
                                const PP_Point* top_left,
                                const PP_Rect* src_rect) {
  if (!top_left)
    return false;

  scoped_refptr<ImageData> image_resource(Resource::GetAs<ImageData>(image));
  if (!image_resource)
    return false;

  QueuedOperation operation(QueuedOperation::PAINT);
  operation.paint_image = image_resource;
  if (!ValidateAndConvertRect(src_rect, image_resource->width(),
                              image_resource->height(),
                              &operation.paint_src_rect))
    return false;

  // Validate the bitmap position using the previously-validated rect, there
  // should be no painted area outside of the image.
  int64 x64 = static_cast<int64>(top_left->x);
  int64 y64 = static_cast<int64>(top_left->y);
  if (x64 + static_cast<int64>(operation.paint_src_rect.x()) < 0 ||
      x64 + static_cast<int64>(operation.paint_src_rect.right()) >
      image_data_->width())
    return false;
  if (y64 + static_cast<int64>(operation.paint_src_rect.y()) < 0 ||
      y64 + static_cast<int64>(operation.paint_src_rect.bottom()) >
      image_data_->height())
    return false;
  operation.paint_x = top_left->x;
  operation.paint_y = top_left->y;

  queued_operations_.push_back(operation);
  return true;
}

bool Graphics2D::Scroll(const PP_Rect* clip_rect, const PP_Point* amount) {
  QueuedOperation operation(QueuedOperation::SCROLL);
  if (!ValidateAndConvertRect(clip_rect,
                              image_data_->width(),
                              image_data_->height(),
                              &operation.scroll_clip_rect))
    return false;

  // If we're being asked to scroll by more than the clip rect size, just
  // ignore this scroll command and say it worked.
  int32 dx = amount->x;
  int32 dy = amount->y;
  if (dx <= -image_data_->width() || dx >= image_data_->width() ||
      dx <= -image_data_->height() || dy >= image_data_->height())
    return true;

  operation.scroll_dx = dx;
  operation.scroll_dy = dy;

  queued_operations_.push_back(operation);
  return false;
}

bool Graphics2D::ReplaceContents(PP_Resource image) {
  scoped_refptr<ImageData> image_resource(Resource::GetAs<ImageData>(image));
  if (!image_resource)
    return false;
  if (image_resource->format() != PP_IMAGEDATAFORMAT_BGRA_PREMUL)
    return false;

  if (image_resource->width() != image_data_->width() ||
      image_resource->height() != image_data_->height())
    return false;

  QueuedOperation operation(QueuedOperation::REPLACE);
  operation.replace_image = image_resource;
  queued_operations_.push_back(operation);

  return true;
}

int32_t Graphics2D::Flush(const PP_CompletionCallback& callback) {
  // Don't allow more than one pending flush at a time.
  if (HasPendingFlush())
    return PP_ERROR_INPROGRESS;

  // TODO(brettw) check that the current thread is not the main one and
  // implement blocking flushes in this case.
  if (!callback.func)
    return PP_ERROR_BADARGUMENT;

  gfx::Rect changed_rect;
  for (size_t i = 0; i < queued_operations_.size(); i++) {
    QueuedOperation& operation = queued_operations_[i];
    gfx::Rect op_rect;
    switch (operation.type) {
      case QueuedOperation::PAINT:
        ExecutePaintImageData(operation.paint_image,
                              operation.paint_x, operation.paint_y,
                              operation.paint_src_rect,
                              &op_rect);
        break;
      case QueuedOperation::SCROLL:
        ExecuteScroll(operation.scroll_clip_rect,
                      operation.scroll_dx, operation.scroll_dy,
                      &op_rect);
        break;
      case QueuedOperation::REPLACE:
        ExecuteReplaceContents(operation.replace_image, &op_rect);
        break;
    }
    changed_rect = changed_rect.Union(op_rect);
  }
  queued_operations_.clear();
  flushed_any_data_ = true;

  // We need the rect to be in terms of the current clip rect of the plugin
  // since that's what will actually be painted. If we issue an invalidate
  // for a clipped-out region, WebKit will do nothing and we won't get any
  // ViewInitiatedPaint/ViewFlushedPaint calls, leaving our callback stranded.
  gfx::Rect visible_changed_rect;
  if (bound_instance_ && !changed_rect.IsEmpty())
    visible_changed_rect = bound_instance_->clip().Intersect(changed_rect);

  if (bound_instance_ && !visible_changed_rect.IsEmpty()) {
    unpainted_flush_callback_.Set(callback);
    bound_instance_->InvalidateRect(visible_changed_rect);
  } else {
    // There's nothing visible to invalidate so just schedule the callback to
    // execute in the next round of the message loop.
    ScheduleOffscreenCallback(FlushCallbackData(callback));
  }
  return PP_ERROR_WOULDBLOCK;
}

bool Graphics2D::ReadImageData(PP_Resource image,
                               const PP_Point* top_left) {
  // Get and validate the image object to paint into.
  scoped_refptr<ImageData> image_resource(Resource::GetAs<ImageData>(image));
  if (!image_resource)
    return false;
  if (image_resource->format() != PP_IMAGEDATAFORMAT_BGRA_PREMUL)
    return false;  // Must be in the right format.

  // Validate the bitmap position.
  int x = top_left->x;
  if (x < 0 ||
      static_cast<int64>(x) + static_cast<int64>(image_resource->width()) >
      image_data_->width())
    return false;
  int y = top_left->y;
  if (y < 0 ||
      static_cast<int64>(y) + static_cast<int64>(image_resource->height()) >
      image_data_->height())
    return false;

  ImageDataAutoMapper auto_mapper(image_resource);
  if (!auto_mapper.is_valid())
    return false;
  skia::PlatformCanvas* dest_canvas = image_resource->mapped_canvas();

  SkIRect src_irect = { x, y,
                        x + image_resource->width(),
                        y + image_resource->height() };
  SkRect dest_rect = { SkIntToScalar(0),
                       SkIntToScalar(0),
                       SkIntToScalar(image_resource->width()),
                       SkIntToScalar(image_resource->height()) };

  // We want to replace the contents of the bitmap rather than blend.
  SkPaint paint;
  paint.setXfermodeMode(SkXfermode::kSrc_Mode);
  dest_canvas->drawBitmapRect(*image_data_->GetMappedBitmap(),
                              &src_irect, dest_rect, &paint);
  return true;
}

bool Graphics2D::BindToInstance(PluginInstance* new_instance) {
  if (bound_instance_ == new_instance)
    return true;  // Rebinding the same device, nothing to do.
  if (bound_instance_ && new_instance)
    return false;  // Can't change a bound device.

  if (!new_instance) {
    // When the device is detached, we'll not get any more paint callbacks so
    // we need to clear the list, but we still want to issue any pending
    // callbacks to the plugin.
    if (!unpainted_flush_callback_.is_null()) {
      ScheduleOffscreenCallback(unpainted_flush_callback_);
      unpainted_flush_callback_.Clear();
    }
    if (!painted_flush_callback_.is_null()) {
      ScheduleOffscreenCallback(painted_flush_callback_);
      painted_flush_callback_.Clear();
    }
  } else if (flushed_any_data_) {
    // Only schedule a paint if this backing store has had any data flushed to
    // it. This is an optimization. A "normal" plugin will first allocated a
    // backing store, bind it, and then execute their normal painting and
    // update loop. If binding a device always invalidated, it would mean we
    // would get one paint for the bind, and one for the first time the plugin
    // actually painted something. By not bothering to schedule an invalidate
    // when an empty device is initially bound, we can save an extra paint for
    // many plugins during the critical page initialization phase.
    new_instance->InvalidateRect(gfx::Rect());
  }

  bound_instance_ = new_instance;
  return true;
}

void Graphics2D::Paint(WebKit::WebCanvas* canvas,
                       const gfx::Rect& plugin_rect,
                       const gfx::Rect& paint_rect) {
  // We're guaranteed to have a mapped canvas since we mapped it in Init().
  const SkBitmap& backing_bitmap = *image_data_->GetMappedBitmap();

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

  // TODO(brettw) bug 56673: do a direct memcpy instead of going through CG
  // if the is_always_opaque_ flag is set.

  CGContextDrawImage(canvas, bounds, image);
  CGContextRestoreGState(canvas);
#else
  SkPaint paint;
  if (is_always_opaque_) {
    // When we know the device is opaque, we can disable blending for slightly
    // more optimized painting.
    paint.setXfermodeMode(SkXfermode::kSrc_Mode);
  }

  gfx::Point origin(plugin_rect.origin().x(), plugin_rect.origin().y());
  canvas->drawBitmap(backing_bitmap,
                     SkIntToScalar(plugin_rect.origin().x()),
                     SkIntToScalar(plugin_rect.origin().y()),
                     &paint);
#endif
}

void Graphics2D::ViewInitiatedPaint() {
  // Move any "unpainted" callback to the painted state. See
  // |unpainted_flush_callback_| in the header for more.
  if (!unpainted_flush_callback_.is_null()) {
    DCHECK(painted_flush_callback_.is_null());
    std::swap(painted_flush_callback_, unpainted_flush_callback_);
  }
}

void Graphics2D::ViewFlushedPaint() {
  // Notify any "painted" callback. See |unpainted_flush_callback_| in the
  // header for more.
  if (!painted_flush_callback_.is_null()) {
    // We must clear this variable before issuing the callback. It will be
    // common for the plugin to issue another invalidate in response to a flush
    // callback, and we don't want to think that a callback is already pending.
    FlushCallbackData callback;
    std::swap(callback, painted_flush_callback_);
    callback.Execute(PP_OK);
  }
}

void Graphics2D::ExecutePaintImageData(ImageData* image,
                                       int x, int y,
                                       const gfx::Rect& src_rect,
                                       gfx::Rect* invalidated_rect) {
  // Ensure the source image is mapped to read from it.
  ImageDataAutoMapper auto_mapper(image);
  if (!auto_mapper.is_valid())
    return;

  // Portion within the source image to cut out.
  SkIRect src_irect = { src_rect.x(), src_rect.y(),
                        src_rect.right(), src_rect.bottom() };

  // Location within the backing store to copy to.
  *invalidated_rect = src_rect;
  invalidated_rect->Offset(x, y);
  SkRect dest_rect = { SkIntToScalar(invalidated_rect->x()),
                       SkIntToScalar(invalidated_rect->y()),
                       SkIntToScalar(invalidated_rect->right()),
                       SkIntToScalar(invalidated_rect->bottom()) };

  // We're guaranteed to have a mapped canvas since we mapped it in Init().
  skia::PlatformCanvas* backing_canvas = image_data_->mapped_canvas();

  // We want to replace the contents of the bitmap rather than blend.
  SkPaint paint;
  paint.setXfermodeMode(SkXfermode::kSrc_Mode);
  backing_canvas->drawBitmapRect(*image->GetMappedBitmap(),
                                 &src_irect, dest_rect, &paint);
}

void Graphics2D::ExecuteScroll(const gfx::Rect& clip, int dx, int dy,
                               gfx::Rect* invalidated_rect) {
  gfx::ScrollCanvas(image_data_->mapped_canvas(),
                    clip, gfx::Point(dx, dy));
  *invalidated_rect = clip;
}

void Graphics2D::ExecuteReplaceContents(ImageData* image,
                                        gfx::Rect* invalidated_rect) {
  image_data_->Swap(image);
  *invalidated_rect = gfx::Rect(0, 0,
                                image_data_->width(), image_data_->height());
}

void Graphics2D::ScheduleOffscreenCallback(const FlushCallbackData& callback) {
  DCHECK(!HasPendingFlush());
  offscreen_flush_pending_ = true;
  MessageLoop::current()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &Graphics2D::ExecuteOffscreenCallback,
                        callback));
}

void Graphics2D::ExecuteOffscreenCallback(FlushCallbackData data) {
  DCHECK(offscreen_flush_pending_);

  // We must clear this flag before issuing the callback. It will be
  // common for the plugin to issue another invalidate in response to a flush
  // callback, and we don't want to think that a callback is already pending.
  offscreen_flush_pending_ = false;
  data.Execute(PP_OK);
}

bool Graphics2D::HasPendingFlush() const {
  return !unpainted_flush_callback_.is_null() ||
         !painted_flush_callback_.is_null() ||
         offscreen_flush_pending_;
}

}  // namespace pepper
