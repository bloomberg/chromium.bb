// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_graphics_2d_impl.h"

#include <iterator>

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "skia/ext/platform_canvas.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/blit.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_image_data_impl.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#endif

namespace webkit {
namespace ppapi {

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

// Converts BGRA <-> RGBA.
void ConvertBetweenBGRAandRGBA(const uint32_t* input,
                               int pixel_length,
                               uint32_t* output) {
  for (int i = 0; i < pixel_length; i++) {
    const unsigned char* pixel_in =
        reinterpret_cast<const unsigned char*>(&input[i]);
    unsigned char* pixel_out = reinterpret_cast<unsigned char*>(&output[i]);
    pixel_out[0] = pixel_in[2];
    pixel_out[1] = pixel_in[1];
    pixel_out[2] = pixel_in[0];
    pixel_out[3] = pixel_in[3];
  }
}

// Converts ImageData from PP_IMAGEDATAFORMAT_BGRA_PREMUL to
// PP_IMAGEDATAFORMAT_RGBA_PREMUL, or reverse. It's assumed that the
// destination image is always mapped (so will have non-NULL data).
void ConvertImageData(PPB_ImageData_Impl* src_image, const SkIRect& src_rect,
                      PPB_ImageData_Impl* dest_image, const SkRect& dest_rect) {
  ImageDataAutoMapper auto_mapper(src_image);

  DCHECK(src_image->format() != dest_image->format());
  DCHECK(PPB_ImageData_Impl::IsImageDataFormatSupported(src_image->format()));
  DCHECK(PPB_ImageData_Impl::IsImageDataFormatSupported(dest_image->format()));

  const SkBitmap* src_bitmap = src_image->GetMappedBitmap();
  const SkBitmap* dest_bitmap = dest_image->GetMappedBitmap();
  if (src_rect.width() == src_image->width() &&
      dest_rect.width() == dest_image->width()) {
    // Fast path if the full line needs to be converted.
    ConvertBetweenBGRAandRGBA(
        src_bitmap->getAddr32(static_cast<int>(src_rect.fLeft),
                              static_cast<int>(src_rect.fTop)),
        src_rect.width() * src_rect.height(),
        dest_bitmap->getAddr32(static_cast<int>(dest_rect.fLeft),
                               static_cast<int>(dest_rect.fTop)));
  } else {
    // Slow path where we convert line by line.
    for (int y = 0; y < src_rect.height(); y++) {
      ConvertBetweenBGRAandRGBA(
          src_bitmap->getAddr32(static_cast<int>(src_rect.fLeft),
                                static_cast<int>(src_rect.fTop + y)),
          src_rect.width(),
          dest_bitmap->getAddr32(static_cast<int>(dest_rect.fLeft),
                                 static_cast<int>(dest_rect.fTop + y)));
    }
  }
}

PP_Resource Create(PP_Instance instance_id,
                   const PP_Size* size,
                   PP_Bool is_always_opaque) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return 0;

  scoped_refptr<PPB_Graphics2D_Impl> context(
      new PPB_Graphics2D_Impl(instance));
  if (!context->Init(size->width, size->height, PPBoolToBool(is_always_opaque)))
    return 0;
  return context->GetReference();
}

PP_Bool IsGraphics2D(PP_Resource resource) {
  return BoolToPPBool(!!Resource::GetAs<PPB_Graphics2D_Impl>(resource));
}

PP_Bool Describe(PP_Resource graphics_2d,
              PP_Size* size,
              PP_Bool* is_always_opaque) {
  scoped_refptr<PPB_Graphics2D_Impl> context(
      Resource::GetAs<PPB_Graphics2D_Impl>(graphics_2d));
  if (!context)
    return PP_FALSE;
  return context->Describe(size, is_always_opaque);
}

void PaintImageData(PP_Resource graphics_2d,
                    PP_Resource image_data,
                    const PP_Point* top_left,
                    const PP_Rect* src_rect) {
  scoped_refptr<PPB_Graphics2D_Impl> context(
      Resource::GetAs<PPB_Graphics2D_Impl>(graphics_2d));
  if (context)
    context->PaintImageData(image_data, top_left, src_rect);
}

void Scroll(PP_Resource graphics_2d,
            const PP_Rect* clip_rect,
            const PP_Point* amount) {
  scoped_refptr<PPB_Graphics2D_Impl> context(
      Resource::GetAs<PPB_Graphics2D_Impl>(graphics_2d));
  if (context)
    context->Scroll(clip_rect, amount);
}

void ReplaceContents(PP_Resource graphics_2d, PP_Resource image_data) {
  scoped_refptr<PPB_Graphics2D_Impl> context(
      Resource::GetAs<PPB_Graphics2D_Impl>(graphics_2d));
  if (context)
    context->ReplaceContents(image_data);
}

int32_t Flush(PP_Resource graphics_2d,
              PP_CompletionCallback callback) {
  scoped_refptr<PPB_Graphics2D_Impl> context(
      Resource::GetAs<PPB_Graphics2D_Impl>(graphics_2d));
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

struct PPB_Graphics2D_Impl::QueuedOperation {
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
  scoped_refptr<PPB_ImageData_Impl> paint_image;
  int paint_x, paint_y;
  gfx::Rect paint_src_rect;

  // Valid when type == SCROLL.
  gfx::Rect scroll_clip_rect;
  int scroll_dx, scroll_dy;

  // Valid when type == REPLACE.
  scoped_refptr<PPB_ImageData_Impl> replace_image;
};

PPB_Graphics2D_Impl::PPB_Graphics2D_Impl(PluginInstance* instance)
    : Resource(instance),
      bound_instance_(NULL),
      flushed_any_data_(false),
      offscreen_flush_pending_(false),
      is_always_opaque_(false) {
}

PPB_Graphics2D_Impl::~PPB_Graphics2D_Impl() {
}

// static
const PPB_Graphics2D* PPB_Graphics2D_Impl::GetInterface() {
  return &ppb_graphics_2d;
}

bool PPB_Graphics2D_Impl::Init(int width, int height, bool is_always_opaque) {
  // The underlying PPB_ImageData_Impl will validate the dimensions.
  image_data_ = new PPB_ImageData_Impl(instance());
  if (!image_data_->Init(PPB_ImageData_Impl::GetNativeImageDataFormat(),
                         width, height, true) ||
      !image_data_->Map()) {
    image_data_ = NULL;
    return false;
  }
  is_always_opaque_ = is_always_opaque;
  return true;
}

PPB_Graphics2D_Impl* PPB_Graphics2D_Impl::AsPPB_Graphics2D_Impl() {
  return this;
}

PP_Bool PPB_Graphics2D_Impl::Describe(PP_Size* size,
                                      PP_Bool* is_always_opaque) {
  size->width = image_data_->width();
  size->height = image_data_->height();
  *is_always_opaque = PP_FALSE;  // TODO(brettw) implement this.
  return PP_TRUE;
}

void PPB_Graphics2D_Impl::PaintImageData(PP_Resource image_data,
                                         const PP_Point* top_left,
                                         const PP_Rect* src_rect) {
  if (!top_left)
    return;

  scoped_refptr<PPB_ImageData_Impl> image_resource(
      Resource::GetAs<PPB_ImageData_Impl>(image_data));
  if (!image_resource)
    return;

  QueuedOperation operation(QueuedOperation::PAINT);
  operation.paint_image = image_resource;
  if (!ValidateAndConvertRect(src_rect, image_resource->width(),
                              image_resource->height(),
                              &operation.paint_src_rect))
    return;

  // Validate the bitmap position using the previously-validated rect, there
  // should be no painted area outside of the image.
  int64 x64 = static_cast<int64>(top_left->x);
  int64 y64 = static_cast<int64>(top_left->y);
  if (x64 + static_cast<int64>(operation.paint_src_rect.x()) < 0 ||
      x64 + static_cast<int64>(operation.paint_src_rect.right()) >
      image_data_->width())
    return;
  if (y64 + static_cast<int64>(operation.paint_src_rect.y()) < 0 ||
      y64 + static_cast<int64>(operation.paint_src_rect.bottom()) >
      image_data_->height())
    return;
  operation.paint_x = top_left->x;
  operation.paint_y = top_left->y;

  queued_operations_.push_back(operation);
}

void PPB_Graphics2D_Impl::Scroll(const PP_Rect* clip_rect,
                                 const PP_Point* amount) {
  QueuedOperation operation(QueuedOperation::SCROLL);
  if (!ValidateAndConvertRect(clip_rect,
                              image_data_->width(),
                              image_data_->height(),
                              &operation.scroll_clip_rect))
    return;

  // If we're being asked to scroll by more than the clip rect size, just
  // ignore this scroll command and say it worked.
  int32 dx = amount->x;
  int32 dy = amount->y;
  if (dx <= -image_data_->width() || dx >= image_data_->width() ||
      dy <= -image_data_->height() || dy >= image_data_->height())
    return;

  operation.scroll_dx = dx;
  operation.scroll_dy = dy;

  queued_operations_.push_back(operation);
}

void PPB_Graphics2D_Impl::ReplaceContents(PP_Resource image_data) {
  scoped_refptr<PPB_ImageData_Impl> image_resource(
      Resource::GetAs<PPB_ImageData_Impl>(image_data));
  if (!image_resource)
    return;
  if (!PPB_ImageData_Impl::IsImageDataFormatSupported(
          image_resource->format()))
    return;

  if (image_resource->width() != image_data_->width() ||
      image_resource->height() != image_data_->height())
    return;

  QueuedOperation operation(QueuedOperation::REPLACE);
  operation.replace_image = image_resource;
  queued_operations_.push_back(operation);
}

int32_t PPB_Graphics2D_Impl::Flush(const PP_CompletionCallback& callback) {
  // Don't allow more than one pending flush at a time.
  if (HasPendingFlush())
    return PP_ERROR_INPROGRESS;

  // TODO(brettw) check that the current thread is not the main one and
  // implement blocking flushes in this case.
  if (!callback.func)
    return PP_ERROR_BADARGUMENT;

  bool nothing_visible = true;
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

    // We need the rect to be in terms of the current clip rect of the plugin
    // since that's what will actually be painted. If we issue an invalidate
    // for a clipped-out region, WebKit will do nothing and we won't get any
    // ViewInitiatedPaint/ViewFlushedPaint calls, leaving our callback stranded.
    gfx::Rect visible_changed_rect;
    if (bound_instance_ && !op_rect.IsEmpty())
      visible_changed_rect = bound_instance_->clip().Intersect(op_rect);

    if (bound_instance_ && !visible_changed_rect.IsEmpty()) {
      if (operation.type == QueuedOperation::SCROLL) {
        bound_instance_->ScrollRect(operation.scroll_dx, operation.scroll_dy,
                                    visible_changed_rect);
      } else {
        bound_instance_->InvalidateRect(visible_changed_rect);
      }
      nothing_visible = false;
    }
  }
  queued_operations_.clear();
  flushed_any_data_ = true;

  if (nothing_visible) {
    // There's nothing visible to invalidate so just schedule the callback to
    // execute in the next round of the message loop.
    ScheduleOffscreenCallback(FlushCallbackData(callback));
  } else {
    unpainted_flush_callback_.Set(callback);
  }
  return PP_ERROR_WOULDBLOCK;
}

bool PPB_Graphics2D_Impl::ReadImageData(PP_Resource image,
                                        const PP_Point* top_left) {
  // Get and validate the image object to paint into.
  scoped_refptr<PPB_ImageData_Impl> image_resource(
      Resource::GetAs<PPB_ImageData_Impl>(image));
  if (!image_resource)
    return false;
  if (!PPB_ImageData_Impl::IsImageDataFormatSupported(
          image_resource->format()))
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

  SkIRect src_irect = { x, y,
                        x + image_resource->width(),
                        y + image_resource->height() };
  SkRect dest_rect = { SkIntToScalar(0),
                       SkIntToScalar(0),
                       SkIntToScalar(image_resource->width()),
                       SkIntToScalar(image_resource->height()) };

  if (image_resource->format() != image_data_->format()) {
    // Convert the image data if the format does not match.
    ConvertImageData(image_data_, src_irect, image_resource.get(), dest_rect);
  } else {
    skia::PlatformCanvas* dest_canvas = image_resource->mapped_canvas();

    // We want to replace the contents of the bitmap rather than blend.
    SkPaint paint;
    paint.setXfermodeMode(SkXfermode::kSrc_Mode);
    dest_canvas->drawBitmapRect(*image_data_->GetMappedBitmap(),
                                &src_irect, dest_rect, &paint);
  }
  return true;
}

bool PPB_Graphics2D_Impl::BindToInstance(PluginInstance* new_instance) {
  if (bound_instance_ == new_instance)
    return true;  // Rebinding the same device, nothing to do.
  if (bound_instance_ && new_instance)
    return false;  // Can't change a bound device.

  if (!new_instance) {
    // When the device is detached, we'll not get any more paint callbacks so
    // we need to clear the list, but we still want to issue any pending
    // callbacks to the plugin.
    if (!unpainted_flush_callback_.is_null()) {
      FlushCallbackData callback;
      std::swap(callback, unpainted_flush_callback_);
      ScheduleOffscreenCallback(callback);
    }
    if (!painted_flush_callback_.is_null()) {
      FlushCallbackData callback;
      std::swap(callback, painted_flush_callback_);
      ScheduleOffscreenCallback(callback);
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

// The |backing_bitmap| must be clipped to the |plugin_rect| to avoid painting
// outside the plugin area. This can happen if the plugin has been resized since
// PaintImageData verified the image is within the plugin size.
void PPB_Graphics2D_Impl::Paint(WebKit::WebCanvas* canvas,
                                const gfx::Rect& plugin_rect,
                                const gfx::Rect& paint_rect) {
  ImageDataAutoMapper auto_mapper(image_data_);
  const SkBitmap& backing_bitmap = *image_data_->GetMappedBitmap();

#if defined(OS_MACOSX)
  SkAutoLockPixels lock(backing_bitmap);

  base::mac::ScopedCFTypeRef<CGDataProviderRef> data_provider(
      CGDataProviderCreateWithData(
          NULL, backing_bitmap.getAddr32(0, 0),
          backing_bitmap.rowBytes() * backing_bitmap.height(), NULL));
  base::mac::ScopedCFTypeRef<CGImageRef> image(
      CGImageCreate(
          backing_bitmap.width(), backing_bitmap.height(),
          8, 32, backing_bitmap.rowBytes(),
          base::mac::GetSystemColorSpace(),
          kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host,
          data_provider, NULL, false, kCGRenderingIntentDefault));

  // Flip the transform
  CGContextSaveGState(canvas);
  float window_height = static_cast<float>(CGBitmapContextGetHeight(canvas));
  CGContextTranslateCTM(canvas, 0, window_height);
  CGContextScaleCTM(canvas, 1.0, -1.0);

  // To avoid painting outside the plugin boundaries and clip instead of
  // scaling, CGContextDrawImage() must draw the full image using |bitmap_rect|
  // but the context must be clipped to the plugin using |bounds|.

  CGRect bitmap_rect;
  bitmap_rect.origin.x = plugin_rect.origin().x();
  bitmap_rect.origin.y = window_height - plugin_rect.origin().y() -
      backing_bitmap.height();
  bitmap_rect.size.width = backing_bitmap.width();
  bitmap_rect.size.height = backing_bitmap.height();

  CGRect bounds;
  bounds.origin.x = plugin_rect.origin().x();
  bounds.origin.y = window_height - plugin_rect.origin().y() -
      plugin_rect.height();
  bounds.size.width = plugin_rect.width();
  bounds.size.height = plugin_rect.height();

  CGContextClipToRect(canvas, bounds);

  // TODO(brettw) bug 56673: do a direct memcpy instead of going through CG
  // if the is_always_opaque_ flag is set. Must ensure bitmap is still clipped.

  CGContextDrawImage(canvas, bitmap_rect, image);
  CGContextRestoreGState(canvas);
#else
  SkPaint paint;
  if (is_always_opaque_) {
    // When we know the device is opaque, we can disable blending for slightly
    // more optimized painting.
    paint.setXfermodeMode(SkXfermode::kSrc_Mode);
  }

  canvas->save();
  SkRect clip_rect = SkRect::MakeXYWH(SkIntToScalar(plugin_rect.origin().x()),
                                      SkIntToScalar(plugin_rect.origin().y()),
                                      SkIntToScalar(plugin_rect.width()),
                                      SkIntToScalar(plugin_rect.height()));
  canvas->clipRect(clip_rect);
  canvas->drawBitmap(backing_bitmap,
                     SkIntToScalar(plugin_rect.x()),
                     SkIntToScalar(plugin_rect.y()),
                     &paint);
  canvas->restore();
#endif
}

void PPB_Graphics2D_Impl::ViewInitiatedPaint() {
  // Move any "unpainted" callback to the painted state. See
  // |unpainted_flush_callback_| in the header for more.
  if (!unpainted_flush_callback_.is_null()) {
    DCHECK(painted_flush_callback_.is_null());
    std::swap(painted_flush_callback_, unpainted_flush_callback_);
  }
}

void PPB_Graphics2D_Impl::ViewFlushedPaint() {
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

void PPB_Graphics2D_Impl::ExecutePaintImageData(PPB_ImageData_Impl* image,
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

  if (image->format() != image_data_->format()) {
    // Convert the image data if the format does not match.
    ConvertImageData(image, src_irect, image_data_, dest_rect);
  } else {
    // We're guaranteed to have a mapped canvas since we mapped it in Init().
    skia::PlatformCanvas* backing_canvas = image_data_->mapped_canvas();

    // We want to replace the contents of the bitmap rather than blend.
    SkPaint paint;
    paint.setXfermodeMode(SkXfermode::kSrc_Mode);
    backing_canvas->drawBitmapRect(*image->GetMappedBitmap(),
                                   &src_irect, dest_rect, &paint);
  }
}

void PPB_Graphics2D_Impl::ExecuteScroll(const gfx::Rect& clip,
                                        int dx, int dy,
                                        gfx::Rect* invalidated_rect) {
  gfx::ScrollCanvas(image_data_->mapped_canvas(),
                    clip, gfx::Point(dx, dy));
  *invalidated_rect = clip;
}

void PPB_Graphics2D_Impl::ExecuteReplaceContents(PPB_ImageData_Impl* image,
                                                 gfx::Rect* invalidated_rect) {
  if (image->format() != image_data_->format()) {
    DCHECK(image->width() == image_data_->width() &&
           image->height() == image_data_->height());
    // Convert the image data if the format does not match.
    SkIRect src_irect = { 0, 0, image->width(), image->height() };
    SkRect dest_rect = { SkIntToScalar(0),
                         SkIntToScalar(0),
                         SkIntToScalar(image_data_->width()),
                         SkIntToScalar(image_data_->height()) };
    ConvertImageData(image, src_irect, image_data_, dest_rect);
  } else {
    // The passed-in image may not be mapped in our process, and we need to
    // guarantee that the current backing store is always mapped.
    if (!image->Map())
      return;
    image_data_->Unmap();
    image_data_->Swap(image);
  }
  *invalidated_rect = gfx::Rect(0, 0,
                                image_data_->width(), image_data_->height());
}

void PPB_Graphics2D_Impl::ScheduleOffscreenCallback(
    const FlushCallbackData& callback) {
  DCHECK(!HasPendingFlush());
  offscreen_flush_pending_ = true;
  MessageLoop::current()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &PPB_Graphics2D_Impl::ExecuteOffscreenCallback,
                        callback));
}

void PPB_Graphics2D_Impl::ExecuteOffscreenCallback(FlushCallbackData data) {
  DCHECK(offscreen_flush_pending_);

  // We must clear this flag before issuing the callback. It will be
  // common for the plugin to issue another invalidate in response to a flush
  // callback, and we don't want to think that a callback is already pending.
  offscreen_flush_pending_ = false;
  data.Execute(PP_OK);
}

bool PPB_Graphics2D_Impl::HasPendingFlush() const {
  return !unpainted_flush_callback_.is_null() ||
         !painted_flush_callback_.is_null() ||
         offscreen_flush_pending_;
}

}  // namespace ppapi
}  // namespace webkit

