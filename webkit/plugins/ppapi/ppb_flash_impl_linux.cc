// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_flash_impl.h"

#include "skia/ext/platform_canvas.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/dev/ppb_font_dev.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "third_party/skia/include/core/SkTemplates.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "webkit/plugins/ppapi/ppb_image_data_impl.h"
#include "webkit/plugins/ppapi/var.h"

namespace webkit {
namespace ppapi {

PP_Bool PPB_Flash_Impl::DrawGlyphs(PP_Instance,
                                   PP_Resource pp_image_data,
                                   const PP_FontDescription_Dev* font_desc,
                                   uint32_t color,
                                   PP_Point position,
                                   PP_Rect clip,
                                   const float transformation[3][3],
                                   uint32_t glyph_count,
                                   const uint16_t glyph_indices[],
                                   const PP_Point glyph_advances[]) {
  scoped_refptr<PPB_ImageData_Impl> image_resource(
      Resource::GetAs<PPB_ImageData_Impl>(pp_image_data));
  if (!image_resource.get())
    return PP_FALSE;
  ImageDataAutoMapper mapper(image_resource);
  if (!mapper.is_valid())
    return PP_FALSE;

  // Set up the typeface.
  scoped_refptr<StringVar> face_name(StringVar::FromPPVar(font_desc->face));
  if (!face_name)
    return PP_FALSE;
  int style = SkTypeface::kNormal;
  if (font_desc->weight >= PP_FONTWEIGHT_BOLD)
    style |= SkTypeface::kBold;
  if (font_desc->italic)
    style |= SkTypeface::kItalic;
  SkTypeface* typeface =
      SkTypeface::CreateFromName(face_name->value().c_str(),
                                 static_cast<SkTypeface::Style>(style));
  if (!typeface)
    return PP_FALSE;

  // Set up the canvas.
  SkCanvas* canvas = image_resource->mapped_canvas();
  canvas->save();

  // Clip is applied in pixels before the transform.
  SkRect clip_rect = { clip.point.x, clip.point.y,
                       clip.point.x + clip.size.width,
                       clip.point.y + clip.size.height };
  canvas->clipRect(clip_rect);

  // -- Do not return early below this. The canvas needs restoring and the
  // typeface will leak if it's not assigned to the paint (it's refcounted and
  // the refcount is currently 0).

  // Convert & set the matrix.
  SkMatrix matrix;
  matrix.set(SkMatrix::kMScaleX, SkFloatToScalar(transformation[0][0]));
  matrix.set(SkMatrix::kMSkewX,  SkFloatToScalar(transformation[0][1]));
  matrix.set(SkMatrix::kMTransX, SkFloatToScalar(transformation[0][2]));
  matrix.set(SkMatrix::kMSkewY,  SkFloatToScalar(transformation[1][0]));
  matrix.set(SkMatrix::kMScaleY, SkFloatToScalar(transformation[1][1]));
  matrix.set(SkMatrix::kMTransY, SkFloatToScalar(transformation[1][2]));
  matrix.set(SkMatrix::kMPersp0, SkFloatToScalar(transformation[2][0]));
  matrix.set(SkMatrix::kMPersp1, SkFloatToScalar(transformation[2][1]));
  matrix.set(SkMatrix::kMPersp2, SkFloatToScalar(transformation[2][2]));
  canvas->concat(matrix);

  SkPaint paint;
  paint.setColor(color);
  paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);
  paint.setAntiAlias(true);
  paint.setHinting(SkPaint::kFull_Hinting);
  paint.setTextSize(SkIntToScalar(font_desc->size));
  paint.setTypeface(typeface);  // Takes a ref and manages lifetime.
  paint.setSubpixelText(true);
  paint.setLCDRenderText(true);

  SkScalar x = SkIntToScalar(position.x);
  SkScalar y = SkIntToScalar(position.y);

  // Build up the skia advances.
  SkAutoSTMalloc<32, SkPoint> storage(glyph_count);
  SkPoint* sk_positions = storage.get();
  for (uint32_t i = 0; i < glyph_count; i++) {
    sk_positions[i].set(x, y);
    x += SkFloatToScalar(glyph_advances[i].x);
    y += SkFloatToScalar(glyph_advances[i].y);
  }

  canvas->drawPosText(glyph_indices, glyph_count * 2, sk_positions, paint);

  canvas->restore();
  return PP_TRUE;
}

}  // namespace ppapi
}  // namespace webkit

