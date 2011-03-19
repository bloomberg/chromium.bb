// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/vector_platform_device.h"

#include <cairo.h>
#include <cairo-ft.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <map>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "skia/ext/bitmap_platform_device.h"
#include "third_party/skia/include/core/SkFontHost.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace {

struct FontInfo {
  SkStream* font_stream;
  FT_Face ft_face;
  cairo_font_face_t* cairo_face;
  cairo_user_data_key_t data_key;
};

typedef std::map<uint32_t, FontInfo> MapFontId2FontInfo;
static base::LazyInstance<MapFontId2FontInfo> g_map_font_id_to_font_info(
    base::LINKER_INITIALIZED);

// Wrapper for FT_Library that handles initialization and cleanup, and allows
// us to use a singleton.
class FtLibrary {
 public:
  FtLibrary() : library_(NULL) {
    FT_Error ft_error = FT_Init_FreeType(&library_);
    if (ft_error) {
      DLOG(ERROR) << "Cannot initialize FreeType library for " \
                  << "VectorPlatformDevice.";
    }
  }

  ~FtLibrary() {
    if (library_) {
      FT_Error ft_error = FT_Done_FreeType(library_);
      library_ = NULL;
      DCHECK_EQ(ft_error, 0);
    }
  }

  FT_Library library() { return library_; }

 private:
  FT_Library library_;
};
static base::LazyInstance<FtLibrary> g_ft_library(base::LINKER_INITIALIZED);

// Verify cairo surface after creation/modification.
bool IsContextValid(cairo_t* context) {
  return cairo_status(context) == CAIRO_STATUS_SUCCESS;
}

}  // namespace

namespace skia {

SkDevice* VectorPlatformDeviceFactory::newDevice(SkCanvas* ignored,
                                                 SkBitmap::Config config,
                                                 int width, int height,
                                                 bool isOpaque,
                                                 bool isForLayer) {
  SkASSERT(config == SkBitmap::kARGB_8888_Config);
  return CreateDevice(NULL, width, height, isOpaque);
}

// static
SkDevice* VectorPlatformDeviceFactory::CreateDevice(cairo_t* context,
                                                    int width, int height,
                                                    bool isOpaque) {
  // TODO(myhuang): Here we might also have similar issues as those on Windows
  // (vector_canvas_win.cc, http://crbug.com/18382 & http://crbug.com/18383).
  // Please note that is_opaque is true when we use this class for printing.
  // Fallback to bitmap when context is NULL.
  if (!isOpaque || NULL == context) {
    return BitmapPlatformDevice::Create(width, height, isOpaque);
  }

  PlatformDevice* device =
    VectorPlatformDevice::create(context, width, height);
  return device;
}

VectorPlatformDevice* VectorPlatformDevice::create(PlatformSurface context,
                                                   int width, int height) {
  SkASSERT(cairo_status(context) == CAIRO_STATUS_SUCCESS);
  SkASSERT(width > 0);
  SkASSERT(height > 0);

  // TODO(myhuang): Can we get rid of the bitmap? In this vectorial device,
  // the content of this bitmap might be meaningless. However, it does occupy
  // lots of memory space.
  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, width, height);

  return new VectorPlatformDevice(context, bitmap);
}

VectorPlatformDevice::VectorPlatformDevice(PlatformSurface context,
                                           const SkBitmap& bitmap)
    : PlatformDevice(bitmap),
      context_(context) {
  SkASSERT(bitmap.getConfig() == SkBitmap::kARGB_8888_Config);

  // Increase the reference count to keep the context alive.
  cairo_reference(context_);

  transform_.reset();
}

VectorPlatformDevice::~VectorPlatformDevice() {
  // Un-ref |context_| since we referenced it in the constructor.
  cairo_destroy(context_);
}

SkDeviceFactory* VectorPlatformDevice::getDeviceFactory() {
  return SkNEW(VectorPlatformDeviceFactory);
}

bool VectorPlatformDevice::IsVectorial() {
  return true;
}

PlatformDevice::PlatformSurface VectorPlatformDevice::beginPlatformPaint() {
  return context_;
}

void VectorPlatformDevice::drawBitmap(const SkDraw& draw,
                                      const SkBitmap& bitmap,
                                      const SkIRect* srcRectOrNull,
                                      const SkMatrix& matrix,
                                      const SkPaint& paint) {
  SkASSERT(bitmap.getConfig() == SkBitmap::kARGB_8888_Config);

  // Load the temporary matrix. This is what will translate, rotate and resize
  // the bitmap.
  SkMatrix actual_transform(transform_);
  actual_transform.preConcat(matrix);
  LoadTransformToContext(actual_transform);

  InternalDrawBitmap(bitmap, 0, 0, paint);

  // Restore the original matrix.
  LoadTransformToContext(transform_);
}

void VectorPlatformDevice::drawDevice(const SkDraw& draw,
                                      SkDevice* device,
                                      int x,
                                      int y,
                                      const SkPaint& paint) {
  SkASSERT(device);

  // TODO(myhuang): We may also have to consider http://b/1183870 .
  drawSprite(draw, device->accessBitmap(false), x, y, paint);
}

void VectorPlatformDevice::drawPaint(const SkDraw& draw,
                                     const SkPaint& paint) {
  // Bypass the current transformation matrix.
  LoadIdentityTransformToContext();

  // TODO(myhuang): Is there a better way to do this?
  SkRect rect;
  rect.fLeft = 0;
  rect.fTop = 0;
  rect.fRight = SkIntToScalar(width() + 1);
  rect.fBottom = SkIntToScalar(height() + 1);
  drawRect(draw, rect, paint);

  // Restore the original matrix.
  LoadTransformToContext(transform_);
}

void VectorPlatformDevice::drawPath(const SkDraw& draw,
                                    const SkPath& path,
                                    const SkPaint& paint,
                                    const SkMatrix* prePathMatrix,
                                    bool pathIsMutable) {
  if (paint.getPathEffect()) {
    // Apply the path effect forehand.
    SkPath path_modified;
    paint.getFillPath(path, &path_modified);

    // Removes the path effect from the temporary SkPaint object.
    SkPaint paint_no_effet(paint);
    SkSafeUnref(paint_no_effet.setPathEffect(NULL));

    // Draw the calculated path.
    drawPath(draw, path_modified, paint_no_effet);
    return;
  }

  // Setup paint color.
  ApplyPaintColor(paint);

  SkPaint::Style style = paint.getStyle();
  // Setup fill style.
  if (style & SkPaint::kFill_Style) {
    ApplyFillStyle(path);
  }

  // Setup stroke style.
  if (style & SkPaint::kStroke_Style) {
    ApplyStrokeStyle(paint);
  }

  // Iterate path verbs.
  // TODO(myhuang): Is there a better way to do this?
  SkPoint current_points[4];
  SkPath::Iter iter(path, false);
  for (SkPath::Verb verb = iter.next(current_points);
       verb != SkPath::kDone_Verb;
       verb = iter.next(current_points)) {
    switch (verb) {
      case SkPath::kMove_Verb: {  // iter.next returns 1 point
        cairo_move_to(context_, current_points[0].fX, current_points[0].fY);
      } break;

      case SkPath::kLine_Verb: {  // iter.next returns 2 points
        cairo_line_to(context_, current_points[1].fX, current_points[1].fY);
      } break;

      case SkPath::kQuad_Verb: {  // iter.next returns 3 points
        // Degree elevation (quadratic to cubic).
        // c1 = (2 * p1 + p0) / 3
        // c2 = (2 * p1 + p2) / 3
        current_points[1].scale(2.);  // p1 *= 2.0;
        SkScalar c1_X = (current_points[1].fX + current_points[0].fX) / 3.;
        SkScalar c1_Y = (current_points[1].fY + current_points[0].fY) / 3.;
        SkScalar c2_X = (current_points[1].fX + current_points[2].fX) / 3.;
        SkScalar c2_Y = (current_points[1].fY + current_points[2].fY) / 3.;
        cairo_curve_to(context_,
                       c1_X, c1_Y,
                       c2_X, c2_Y,
                       current_points[2].fX, current_points[2].fY);
      } break;

      case SkPath::kCubic_Verb: {  // iter.next returns 4 points
        cairo_curve_to(context_,
                       current_points[1].fX, current_points[1].fY,
                       current_points[2].fX, current_points[2].fY,
                       current_points[3].fX, current_points[3].fY);
      } break;

      case SkPath::kClose_Verb: {  // iter.next returns 1 point (the last pt).
        cairo_close_path(context_);
      } break;

      case SkPath::kDone_Verb: {  // iter.next returns 0 points
      } break;

      default: {
        // Should not reach here!
        SkASSERT(false);
      } break;
    }
  }

  DoPaintStyle(paint);
}

void VectorPlatformDevice::drawPoints(const SkDraw& draw,
                                      SkCanvas::PointMode mode,
                                      size_t count,
                                      const SkPoint pts[],
                                      const SkPaint& paint) {
  SkASSERT(pts);

  if (!count)
    return;

  // Setup paint color.
  ApplyPaintColor(paint);

  // Setup stroke style.
  ApplyStrokeStyle(paint);

  switch (mode) {
    case SkCanvas::kPoints_PointMode: {
      // There is a bug in Cairo that it won't draw anything when using some
      // specific caps, e.g. SkPaint::kSquare_Cap. This is because Cairo does
      // not have enough/ambiguous direction information. One possible work-
      // around is to draw a really short line.
      for (size_t i = 0; i < count; ++i) {
        double x = pts[i].fX;
        double y = pts[i].fY;
        cairo_move_to(context_, x, y);
        cairo_line_to(context_, x+.01, y);
      }
    } break;

    case SkCanvas::kLines_PointMode: {
      if (count % 2) {
        SkASSERT(false);
        return;
      }

      for (size_t i = 0; i < count >> 1; ++i) {
        double x1 = pts[i << 1].fX;
        double y1 = pts[i << 1].fY;
        double x2 = pts[(i << 1) + 1].fX;
        double y2 = pts[(i << 1) + 1].fY;
        cairo_move_to(context_, x1, y1);
        cairo_line_to(context_, x2, y2);
      }
    } break;

    case SkCanvas::kPolygon_PointMode: {
      double x = pts[0].fX;
      double y = pts[0].fY;
      cairo_move_to(context_, x, y);
      for (size_t i = 1; i < count; ++i) {
        x = pts[i].fX;
        y = pts[i].fY;
        cairo_line_to(context_, x, y);
      }
    } break;

    default:
      SkASSERT(false);
      return;
  }
  cairo_stroke(context_);
}

// TODO(myhuang): Embed fonts/texts into PDF surface.
// Please NOTE that len records text's length in byte, not uint16_t.
void VectorPlatformDevice::drawPosText(const SkDraw& draw,
                                       const void* text,
                                       size_t len,
                                       const SkScalar pos[],
                                       SkScalar constY,
                                       int scalarsPerPos,
                                       const SkPaint& paint) {
  SkASSERT(text);
  SkASSERT(pos);
  SkASSERT(paint.getTextEncoding() == SkPaint::kGlyphID_TextEncoding);
  // Each pos should contain either only x, or (x, y).
  SkASSERT((scalarsPerPos == 1) || (scalarsPerPos == 2));

  if (!len)
    return;

  // Text color.
  ApplyPaintColor(paint);

  const uint16_t* glyph_ids = static_cast<const uint16_t*>(text);

  // The style is either kFill_Style or kStroke_Style.
  if (paint.getStyle() & SkPaint::kStroke_Style) {
    ApplyStrokeStyle(paint);

    // Draw each glyph by its path.
    for (size_t i = 0; i < len / sizeof(uint16_t); ++i) {
      uint16_t glyph_id = glyph_ids[i];
      SkPath textPath;
      paint.getTextPath(&glyph_id,
                        sizeof(uint16_t),
                        pos[i * scalarsPerPos],
                        (scalarsPerPos == 1) ?
                            constY :
                            pos[i * scalarsPerPos + 1],
                        &textPath);
      drawPath(draw, textPath, paint);
    }
  } else {  // kFill_Style.
    // Selects correct font.
    if (!SelectFontById(paint.getTypeface()->uniqueID())) {
      SkASSERT(false);
      return;
    }
    cairo_set_font_size(context_, paint.getTextSize());

    // Draw glyphs.
    for (size_t i = 0; i < len / sizeof(uint16_t); ++i) {
      uint16_t glyph_id = glyph_ids[i];

      cairo_glyph_t glyph;
      glyph.index = glyph_id;
      glyph.x = pos[i * scalarsPerPos];
      glyph.y = (scalarsPerPos == 1) ? constY : pos[i * scalarsPerPos + 1];

      cairo_show_glyphs(context_, &glyph, 1);
    }
  }
}

void VectorPlatformDevice::drawRect(const SkDraw& draw,
                                    const SkRect& rect,
                                    const SkPaint& paint) {
  if (paint.getPathEffect()) {
    // Draw a path instead.
    SkPath path_orginal;
    path_orginal.addRect(rect);

    // Apply the path effect to the rect.
    SkPath path_modified;
    paint.getFillPath(path_orginal, &path_modified);

    // Removes the path effect from the temporary SkPaint object.
    SkPaint paint_no_effet(paint);
    SkSafeUnref(paint_no_effet.setPathEffect(NULL));

    // Draw the calculated path.
    drawPath(draw, path_modified, paint_no_effet);
    return;
  }

  // Setup color.
  ApplyPaintColor(paint);

  // Setup stroke style.
  ApplyStrokeStyle(paint);

  // Draw rectangle.
  cairo_rectangle(context_,
                  rect.fLeft, rect.fTop,
                  rect.fRight - rect.fLeft, rect.fBottom - rect.fTop);

  DoPaintStyle(paint);
}

void VectorPlatformDevice::drawSprite(const SkDraw& draw,
                                      const SkBitmap& bitmap,
                                      int x, int y,
                                      const SkPaint& paint) {
  SkASSERT(bitmap.getConfig() == SkBitmap::kARGB_8888_Config);

  LoadIdentityTransformToContext();

  InternalDrawBitmap(bitmap, x, y, paint);

  // Restore the original matrix.
  LoadTransformToContext(transform_);
}

void VectorPlatformDevice::drawText(const SkDraw& draw,
                                    const void* text,
                                    size_t byteLength,
                                    SkScalar x,
                                    SkScalar y,
                                    const SkPaint& paint) {
  // This function isn't used in the code. Verify this assumption.
  SkASSERT(false);
}


void VectorPlatformDevice::drawTextOnPath(const SkDraw& draw,
                                          const void* text,
                                          size_t len,
                                          const SkPath& path,
                                          const SkMatrix* matrix,
                                          const SkPaint& paint) {
  // This function isn't used in the code. Verify this assumption.
  SkASSERT(false);
}

void VectorPlatformDevice::drawVertices(const SkDraw& draw,
                                        SkCanvas::VertexMode vmode,
                                        int vertexCount,
                                        const SkPoint vertices[],
                                        const SkPoint texs[],
                                        const SkColor colors[],
                                        SkXfermode* xmode,
                                        const uint16_t indices[],
                                        int indexCount,
                                        const SkPaint& paint) {
  // This function isn't used in the code. Verify this assumption.
  SkASSERT(false);
}

void VectorPlatformDevice::setMatrixClip(const SkMatrix& transform,
                                         const SkRegion& region,
                                         const SkClipStack&) {
  clip_region_ = region;
  if (!clip_region_.isEmpty())
    LoadClipRegion(clip_region_);

  transform_ = transform;
  LoadTransformToContext(transform_);
}

void VectorPlatformDevice::ApplyPaintColor(const SkPaint& paint) {
  SkColor color = paint.getColor();
  double a = static_cast<double>(SkColorGetA(color)) / 255.;
  double r = static_cast<double>(SkColorGetR(color)) / 255.;
  double g = static_cast<double>(SkColorGetG(color)) / 255.;
  double b = static_cast<double>(SkColorGetB(color)) / 255.;

  cairo_set_source_rgba(context_, r, g, b, a);
}

void VectorPlatformDevice::ApplyFillStyle(const SkPath& path) {
  // Setup fill style.
  // TODO(myhuang): Cairo does NOT support all skia fill rules!!
  cairo_set_fill_rule(context_,
                      static_cast<cairo_fill_rule_t>(path.getFillType()));
}

void VectorPlatformDevice::ApplyStrokeStyle(const SkPaint& paint) {
  // Line width.
  cairo_set_line_width(context_, paint.getStrokeWidth());

  // Line join.
  cairo_set_line_join(context_,
                      static_cast<cairo_line_join_t>(paint.getStrokeJoin()));

  // Line cap.
  cairo_set_line_cap(context_,
                     static_cast<cairo_line_cap_t>(paint.getStrokeCap()));
}

void VectorPlatformDevice::DoPaintStyle(const SkPaint& paint) {
  SkPaint::Style style = paint.getStyle();

  switch (style) {
    case SkPaint::kFill_Style: {
      cairo_fill(context_);
    } break;

    case SkPaint::kStroke_Style: {
      cairo_stroke(context_);
    } break;

    case SkPaint::kStrokeAndFill_Style: {
      cairo_fill_preserve(context_);
      cairo_stroke(context_);
    } break;

    default:
      SkASSERT(false);
  }
}

void VectorPlatformDevice::InternalDrawBitmap(const SkBitmap& bitmap,
                                              int x, int y,
                                              const SkPaint& paint) {
  SkASSERT(bitmap.getConfig() == SkBitmap::kARGB_8888_Config);

  unsigned char alpha = paint.getAlpha();

  if (alpha == 0)
    return;

  int src_size_x = bitmap.width();
  int src_size_y = bitmap.height();

  if (!src_size_x || !src_size_y)
    return;

  SkAutoLockPixels image_lock(bitmap);

  cairo_surface_t* bitmap_surface =
      cairo_image_surface_create_for_data(
          reinterpret_cast<unsigned char*>(bitmap.getPixels()),
          CAIRO_FORMAT_ARGB32, src_size_x, src_size_y, bitmap.rowBytes());

  cairo_set_source_surface(context_, bitmap_surface, x, y);
  cairo_paint_with_alpha(context_, static_cast<double>(alpha) / 255.);

  cairo_surface_destroy(bitmap_surface);
}

void VectorPlatformDevice::LoadClipRegion(const SkRegion& clip) {
  cairo_reset_clip(context_);

  LoadIdentityTransformToContext();

  // TODO(myhuang): Support non-rect clips.
  SkIRect bounding = clip.getBounds();
  cairo_rectangle(context_, bounding.fLeft, bounding.fTop,
                  bounding.fRight - bounding.fLeft,
                  bounding.fBottom - bounding.fTop);
  cairo_clip(context_);

  // Restore the original matrix.
  LoadTransformToContext(transform_);
}

void VectorPlatformDevice::LoadIdentityTransformToContext() {
  SkMatrix identity;
  identity.reset();
  LoadTransformToContext(identity);
}

void VectorPlatformDevice::LoadTransformToContext(const SkMatrix& matrix) {
  cairo_matrix_t m;
  m.xx = matrix[SkMatrix::kMScaleX];
  m.xy = matrix[SkMatrix::kMSkewX];
  m.x0 = matrix[SkMatrix::kMTransX];
  m.yx = matrix[SkMatrix::kMSkewY];
  m.yy = matrix[SkMatrix::kMScaleY];
  m.y0 = matrix[SkMatrix::kMTransY];
  cairo_set_matrix(context_, &m);
}

bool VectorPlatformDevice::SelectFontById(uint32_t font_id) {
  DCHECK(IsContextValid(context_));
  DCHECK(SkFontHost::ValidFontID(font_id));

  FtLibrary* ft_library = g_ft_library.Pointer();
  if (!ft_library->library())
    return false;

  // Checks if we have a cache hit.
  MapFontId2FontInfo* g_font_cache = g_map_font_id_to_font_info.Pointer();
  DCHECK(g_font_cache);

  MapFontId2FontInfo::iterator it = g_font_cache->find(font_id);
  if (it != g_font_cache->end()) {
    cairo_set_font_face(context_, it->second.cairo_face);
    if (IsContextValid(context_)) {
      return true;
    } else {
      NOTREACHED() << "Cannot set font face in Cairo!";
      return false;
    }
  }

  // Cache missed. We need to load and create the font.
  FontInfo new_font_info = {0};
  new_font_info.font_stream = SkFontHost::OpenStream(font_id);
  DCHECK(new_font_info.font_stream);
  size_t stream_size = new_font_info.font_stream->getLength();
  DCHECK(stream_size) << "The Font stream has nothing!";

  FT_Error ft_error = FT_New_Memory_Face(
      ft_library->library(),
      static_cast<FT_Byte*>(
          const_cast<void*>(new_font_info.font_stream->getMemoryBase())),
      stream_size,
      0,
      &new_font_info.ft_face);

  if (ft_error) {
    new_font_info.font_stream->unref();
    DLOG(ERROR) << "Cannot create FT_Face!";
    SkASSERT(false);
    return false;
  }

  new_font_info.cairo_face = cairo_ft_font_face_create_for_ft_face(
      new_font_info.ft_face, 0);
  DCHECK(new_font_info.cairo_face) << "Cannot create font in Cairo!";

  // Manage |new_font_info.ft_face|'s life by Cairo.
  cairo_status_t status = cairo_font_face_set_user_data(
      new_font_info.cairo_face,
      &new_font_info.data_key,
      new_font_info.ft_face,
      reinterpret_cast<cairo_destroy_func_t>(FT_Done_Face));

  if (status != CAIRO_STATUS_SUCCESS) {
    DLOG(ERROR) << "Cannot set font's user data in Cairo!";
    cairo_font_face_destroy(new_font_info.cairo_face);
    FT_Done_Face(new_font_info.ft_face);
    new_font_info.font_stream->unref();
    SkASSERT(false);
    return false;
  }

  // Inserts |new_font_info| info |g_font_cache|.
  (*g_font_cache)[font_id] = new_font_info;

  cairo_set_font_face(context_, new_font_info.cairo_face);
  if (IsContextValid(context_)) {
    return true;
  }

  DLOG(ERROR) << "Connot set font face in Cairo!";
  return false;
}

// static
void VectorPlatformDevice::ClearFontCache() {
  MapFontId2FontInfo* g_font_cache = g_map_font_id_to_font_info.Pointer();
  DCHECK(g_font_cache);

  for (MapFontId2FontInfo::iterator it = g_font_cache->begin();
       it !=g_font_cache->end();
       ++it) {
    DCHECK(it->second.cairo_face);
    DCHECK(it->second.font_stream);

    cairo_font_face_destroy(it->second.cairo_face);
    // |it->second.ft_face| is handled by Cairo.
    it->second.font_stream->unref();
  }
  g_font_cache->clear();
}

}  // namespace skia
