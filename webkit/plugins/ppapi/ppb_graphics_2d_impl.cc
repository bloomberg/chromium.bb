// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_graphics_2d_impl.h"

#include <iterator>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "skia/ext/platform_canvas.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/thunk.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/blit.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"
#include "ui/gfx/skia_util.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/gfx_conversion.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_image_data_impl.h"
#include "webkit/plugins/ppapi/resource_helper.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#endif

using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_ImageData_API;
using ppapi::TrackedCallback;

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

// Scale the rectangle, taking care to round coordinates outward so a
// rectangle scaled down then scaled back up by the inverse scale would
// fully contain the entire area affected by the original rectangle.
gfx::Rect ScaleRectBounds(const gfx::Rect& rect, float scale) {
  int left = static_cast<int>(floorf(rect.x() * scale));
  int top = static_cast<int>(floorf(rect.y() * scale));
  int right = static_cast<int>(ceilf((rect.x() + rect.width()) * scale));
  int bottom = static_cast<int>(ceilf((rect.y() + rect.height()) * scale));
  return gfx::Rect(left, top, right - left, bottom - top);
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

PPB_Graphics2D_Impl::PPB_Graphics2D_Impl(PP_Instance instance)
    : Resource(::ppapi::OBJECT_IS_IMPL, instance),
      bound_instance_(NULL),
      offscreen_flush_pending_(false),
      is_always_opaque_(false),
      scale_(1.0f),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

PPB_Graphics2D_Impl::~PPB_Graphics2D_Impl() {
  // LastPluginRefWasDeleted should have aborted all pending callbacks.
  DCHECK(painted_flush_callback_.is_null());
  DCHECK(unpainted_flush_callback_.is_null());
}

// static
PP_Resource PPB_Graphics2D_Impl::Create(PP_Instance instance,
                                        const PP_Size& size,
                                        PP_Bool is_always_opaque) {
  scoped_refptr<PPB_Graphics2D_Impl> graphics_2d(
      new PPB_Graphics2D_Impl(instance));
  if (!graphics_2d->Init(size.width, size.height,
                         PPBoolToBool(is_always_opaque))) {
    return 0;
  }
  return graphics_2d->GetReference();
}

bool PPB_Graphics2D_Impl::Init(int width, int height, bool is_always_opaque) {
  // The underlying PPB_ImageData_Impl will validate the dimensions.
  image_data_ = new PPB_ImageData_Impl(pp_instance(),
                                       PPB_ImageData_Impl::PLATFORM);
  if (!image_data_->Init(PPB_ImageData_Impl::GetNativeImageDataFormat(),
                         width, height, true) ||
      !image_data_->Map()) {
    image_data_ = NULL;
    return false;
  }
  is_always_opaque_ = is_always_opaque;
  scale_ = 1.0f;
  return true;
}

::ppapi::thunk::PPB_Graphics2D_API*
PPB_Graphics2D_Impl::AsPPB_Graphics2D_API() {
  return this;
}

void PPB_Graphics2D_Impl::LastPluginRefWasDeleted() {
  Resource::LastPluginRefWasDeleted();

  // Abort any pending callbacks.
  unpainted_flush_callback_.PostAbort();
  painted_flush_callback_.PostAbort();
}

PP_Bool PPB_Graphics2D_Impl::Describe(PP_Size* size,
                                      PP_Bool* is_always_opaque) {
  size->width = image_data_->width();
  size->height = image_data_->height();
  *is_always_opaque = PP_FromBool(is_always_opaque_);
  return PP_TRUE;
}

void PPB_Graphics2D_Impl::PaintImageData(PP_Resource image_data,
                                         const PP_Point* top_left,
                                         const PP_Rect* src_rect) {
  if (!top_left)
    return;

  EnterResourceNoLock<PPB_ImageData_API> enter(image_data, true);
  if (enter.failed()) {
    Log(PP_LOGLEVEL_ERROR,
        "PPB_Graphics2D.PaintImageData: Bad image resource.");
    return;
  }
  PPB_ImageData_Impl* image_resource =
      static_cast<PPB_ImageData_Impl*>(enter.object());

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
      dy <= -image_data_->height() || dy >= image_data_->height()) {
    Log(PP_LOGLEVEL_ERROR,
        "PPB_Graphics2D.Scroll: Scroll amount is larger than image size.");
    return;
  }

  operation.scroll_dx = dx;
  operation.scroll_dy = dy;

  queued_operations_.push_back(operation);
}

void PPB_Graphics2D_Impl::ReplaceContents(PP_Resource image_data) {
  EnterResourceNoLock<PPB_ImageData_API> enter(image_data, true);
  if (enter.failed())
    return;
  PPB_ImageData_Impl* image_resource =
      static_cast<PPB_ImageData_Impl*>(enter.object());

  if (!PPB_ImageData_Impl::IsImageDataFormatSupported(
          image_resource->format())) {
    Log(PP_LOGLEVEL_ERROR,
        "PPB_Graphics2D.ReplaceContents: Image data format is not supported.");
    return;
  }

  if (image_resource->width() != image_data_->width() ||
      image_resource->height() != image_data_->height()) {
    Log(PP_LOGLEVEL_ERROR,
        "PPB_Graphics2D.ReplaceContents: Image size doesn't match "
        "Graphics2D size.");
    return;
  }

  QueuedOperation operation(QueuedOperation::REPLACE);
  operation.replace_image = image_resource;
  queued_operations_.push_back(operation);
}

int32_t PPB_Graphics2D_Impl::Flush(scoped_refptr<TrackedCallback> callback,
                                   PP_Resource* old_image_data) {
  TRACE_EVENT0("pepper", "PPB_Graphics2D_Impl::Flush");
  // Don't allow more than one pending flush at a time.
  if (HasPendingFlush())
    return PP_ERROR_INPROGRESS;

  bool done_replace_contents = false;
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
        // Since the out parameter |old_image_data| takes ownership of the
        // reference, if there are more than one ReplaceContents calls queued
        // the first |old_image_data| will get overwritten and leaked. So we
        // only supply this for the first call.
        ExecuteReplaceContents(operation.replace_image, &op_rect,
                               done_replace_contents ? NULL : old_image_data);
        done_replace_contents = true;
        break;
    }

    // For correctness with accelerated compositing, we must issue an invalidate
    // on the full op_rect even if it is partially or completely off-screen.
    // However, if we issue an invalidate for a clipped-out region, WebKit will
    // do nothing and we won't get any ViewWillInitiatePaint/ViewFlushedPaint
    // calls, leaving our callback stranded. So we still need to check whether
    // the repainted area is visible to determine how to deal with the callback.
    if (bound_instance_ && !op_rect.IsEmpty()) {
      gfx::Point scroll_delta(operation.scroll_dx, operation.scroll_dy);
      if (!ConvertToLogicalPixels(scale_,
                                  &op_rect,
                                  operation.type == QueuedOperation::SCROLL ?
                                      &scroll_delta : NULL)) {
        // Conversion requires falling back to InvalidateRect.
        operation.type = QueuedOperation::PAINT;
      }

      // Set |nothing_visible| to false if the change overlaps the visible area.
      gfx::Rect visible_changed_rect =
          PP_ToGfxRect(bound_instance_->view_data().clip_rect).
          Intersect(op_rect);
      if (!visible_changed_rect.IsEmpty())
        nothing_visible = false;

      // Notify the plugin of the entire change (op_rect), even if it is
      // partially or completely off-screen.
      if (operation.type == QueuedOperation::SCROLL) {
        bound_instance_->ScrollRect(scroll_delta.x(), scroll_delta.y(),
                                    op_rect);
      } else {
        bound_instance_->InvalidateRect(op_rect);
      }
    }
  }
  queued_operations_.clear();

  if (nothing_visible) {
    // There's nothing visible to invalidate so just schedule the callback to
    // execute in the next round of the message loop.
    ScheduleOffscreenCallback(FlushCallbackData(callback));
  } else {
    unpainted_flush_callback_.Set(callback);
  }
  return PP_OK_COMPLETIONPENDING;
}

bool PPB_Graphics2D_Impl::SetScale(float scale) {
  if (scale > 0.0f) {
    scale_ = scale;
    return true;
  }

  return false;
}

float PPB_Graphics2D_Impl::GetScale() {
  return scale_;
}

bool PPB_Graphics2D_Impl::ReadImageData(PP_Resource image,
                                        const PP_Point* top_left) {
  // Get and validate the image object to paint into.
  EnterResourceNoLock<PPB_ImageData_API> enter(image, true);
  if (enter.failed())
    return false;
  PPB_ImageData_Impl* image_resource =
      static_cast<PPB_ImageData_Impl*>(enter.object());
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
    ConvertImageData(image_data_, src_irect, image_resource, dest_rect);
  } else {
    SkCanvas* dest_canvas = image_resource->GetCanvas();

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
  } else {
    // Devices being replaced, redraw the plugin.
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
  TRACE_EVENT0("pepper", "PPB_Graphics2D_Impl::Paint");
  ImageDataAutoMapper auto_mapper(image_data_);
  const SkBitmap& backing_bitmap = *image_data_->GetMappedBitmap();

#if defined(OS_MACOSX) && !defined(USE_SKIA)
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
  gfx::ScopedCGContextSaveGState save_gstate(canvas)
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
  // TODO(yzshen): We should take |paint_rect| into consideration as well.
  CGContextClipToRect(canvas, bounds);

  // TODO(jhorwich) Figure out if this code is even active anymore, and if so
  // how to properly handle scaling.
  DCHECK_EQ(1.0f, scale_);

  // TODO(brettw) bug 56673: do a direct memcpy instead of going through CG
  // if the is_always_opaque_ flag is set. Must ensure bitmap is still clipped.

  CGContextDrawImage(canvas, bitmap_rect, image);
#else
  gfx::Rect invalidate_rect = plugin_rect.Intersect(paint_rect);
  SkRect sk_invalidate_rect = gfx::RectToSkRect(invalidate_rect);
  SkAutoCanvasRestore auto_restore(canvas, true);
  canvas->clipRect(sk_invalidate_rect);
  gfx::Size pixel_image_size(image_data_->width(), image_data_->height());
  gfx::Size image_size = pixel_image_size.Scale(scale_);

  PluginInstance* plugin_instance = ResourceHelper::GetPluginInstance(this);
  if (!plugin_instance)
    return;
  if (plugin_instance->IsFullPagePlugin()) {
    // When we're resizing a window with a full-frame plugin, the plugin may
    // not yet have bound a new device, which will leave parts of the
    // background exposed if the window is getting larger. We want this to
    // show white (typically less jarring) rather than black or uninitialized.
    // We don't do this for non-full-frame plugins since we specifically want
    // the page background to show through.
    SkAutoCanvasRestore auto_restore(canvas, true);
    SkRect image_data_rect =
        gfx::RectToSkRect(gfx::Rect(plugin_rect.origin(), image_size));
    canvas->clipRect(image_data_rect, SkRegion::kDifference_Op);

    SkPaint paint;
    paint.setXfermodeMode(SkXfermode::kSrc_Mode);
    paint.setColor(SK_ColorWHITE);
    canvas->drawRect(sk_invalidate_rect, paint);
  }

  SkBitmap image;
  // Copy to device independent bitmap when target canvas doesn't support
  // platform paint.
  if (!skia::SupportsPlatformPaint(canvas))
    backing_bitmap.copyTo(&image, SkBitmap::kARGB_8888_Config);
  else
    image = backing_bitmap;

  SkPaint paint;
  if (is_always_opaque_) {
    // When we know the device is opaque, we can disable blending for slightly
    // more optimized painting.
    paint.setXfermodeMode(SkXfermode::kSrc_Mode);
  }

  SkPoint origin;
  origin.set(SkIntToScalar(plugin_rect.x()), SkIntToScalar(plugin_rect.y()));

  SkPoint pixel_origin = origin;
  if (scale_ != 1.0f && scale_ > 0.0f) {
    float inverse_scale = 1.0f / scale_;
    pixel_origin.scale(inverse_scale);
    canvas->scale(scale_, scale_);
  }
  canvas->drawBitmap(image, pixel_origin.x(), pixel_origin.y(), &paint);
#endif
}

void PPB_Graphics2D_Impl::ViewWillInitiatePaint() {
  // Move any "unpainted" callback to the painted state. See
  // |unpainted_flush_callback_| in the header for more.
  if (!unpainted_flush_callback_.is_null()) {
    DCHECK(painted_flush_callback_.is_null());
    std::swap(painted_flush_callback_, unpainted_flush_callback_);
  }
}

void PPB_Graphics2D_Impl::ViewInitiatedPaint() {
}

void PPB_Graphics2D_Impl::ViewFlushedPaint() {
  TRACE_EVENT0("pepper", "PPB_Graphics2D_Impl::ViewFlushedPaint");
  // Notify any "painted" callback. See |unpainted_flush_callback_| in the
  // header for more.
  if (!painted_flush_callback_.is_null())
    painted_flush_callback_.Execute(PP_OK);
}

// static
bool PPB_Graphics2D_Impl::ConvertToLogicalPixels(float scale,
                                                 gfx::Rect* op_rect,
                                                 gfx::Point* delta) {
  if (scale == 1.0f || scale <= 0.0f)
    return true;

  gfx::Rect original_rect = *op_rect;
  *op_rect = ScaleRectBounds(*op_rect, scale);
  if (delta) {
    gfx::Point original_delta = *delta;
    float inverse_scale = 1.0f / scale;
    *delta = delta->Scale(scale);
    if (original_rect != ScaleRectBounds(*op_rect, inverse_scale) ||
        original_delta != delta->Scale(inverse_scale)) {
      return false;
    }
  }

  return true;
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
    SkCanvas* backing_canvas = image_data_->GetCanvas();

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
  gfx::ScrollCanvas(image_data_->GetCanvas(),
                    clip, gfx::Point(dx, dy));
  *invalidated_rect = clip;
}

void PPB_Graphics2D_Impl::ExecuteReplaceContents(PPB_ImageData_Impl* image,
                                                 gfx::Rect* invalidated_rect,
                                                 PP_Resource* old_image_data) {
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

    if (old_image_data)
      *old_image_data = image_data_->GetReference();
    image_data_ = image;
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
      base::Bind(&PPB_Graphics2D_Impl::ExecuteOffscreenCallback,
                 weak_ptr_factory_.GetWeakPtr(),
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
