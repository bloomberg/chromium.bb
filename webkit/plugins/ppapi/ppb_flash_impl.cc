// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_flash_impl.h"

#include <string>
#include <vector>

#include "googleurl/src/gurl.h"
#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/c/trusted/ppb_browser_font_trusted.h"
#include "ppapi/shared_impl/time_conversion.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_image_data_api.h"
#include "ppapi/thunk/ppb_url_request_info_api.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "third_party/skia/include/core/SkTemplates.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginContainer.h"
#include "ui/gfx/rect.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/host_globals.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/resource_helper.h"
#include "webkit/plugins/ppapi/ppb_image_data_impl.h"

using ppapi::PPTimeToTime;
using ppapi::StringVar;
using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_ImageData_API;
using ppapi::thunk::PPB_URLRequestInfo_API;

namespace webkit {
namespace ppapi {

PPB_Flash_Impl::PPB_Flash_Impl(PluginInstance* instance)
    : instance_(instance) {
}

PPB_Flash_Impl::~PPB_Flash_Impl() {
}

void PPB_Flash_Impl::SetInstanceAlwaysOnTop(PP_Instance instance,
                                            PP_Bool on_top) {
  instance_->set_always_on_top(PP_ToBool(on_top));
}

PP_Bool PPB_Flash_Impl::DrawGlyphs(
    PP_Instance instance,
    PP_Resource pp_image_data,
    const PP_BrowserFont_Trusted_Description* font_desc,
    uint32_t color,
    const PP_Point* position,
    const PP_Rect* clip,
    const float transformation[3][3],
    PP_Bool allow_subpixel_aa,
    uint32_t glyph_count,
    const uint16_t glyph_indices[],
    const PP_Point glyph_advances[]) {
  EnterResourceNoLock<PPB_ImageData_API> enter(pp_image_data, true);
  if (enter.failed())
    return PP_FALSE;
  PPB_ImageData_Impl* image_resource =
      static_cast<PPB_ImageData_Impl*>(enter.object());

  ImageDataAutoMapper mapper(image_resource);
  if (!mapper.is_valid())
    return PP_FALSE;

  // Set up the typeface.
  StringVar* face_name = StringVar::FromPPVar(font_desc->face);
  if (!face_name)
    return PP_FALSE;
  int style = SkTypeface::kNormal;
  if (font_desc->weight >= PP_BROWSERFONT_TRUSTED_WEIGHT_BOLD)
    style |= SkTypeface::kBold;
  if (font_desc->italic)
    style |= SkTypeface::kItalic;
  skia::RefPtr<SkTypeface> typeface = skia::AdoptRef(
      SkTypeface::CreateFromName(face_name->value().c_str(),
                                 static_cast<SkTypeface::Style>(style)));
  if (!typeface)
    return PP_FALSE;

  // Set up the canvas.
  SkCanvas* canvas = image_resource->GetPlatformCanvas();
  SkAutoCanvasRestore acr(canvas, true);

  // Clip is applied in pixels before the transform.
  SkRect clip_rect = { SkIntToScalar(clip->point.x),
                       SkIntToScalar(clip->point.y),
                       SkIntToScalar(clip->point.x + clip->size.width),
                       SkIntToScalar(clip->point.y + clip->size.height) };
  canvas->clipRect(clip_rect);

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
  paint.setTypeface(typeface.get());  // Takes a ref and manages lifetime.
  if (allow_subpixel_aa) {
    paint.setSubpixelText(true);
    paint.setLCDRenderText(true);
  }

  SkScalar x = SkIntToScalar(position->x);
  SkScalar y = SkIntToScalar(position->y);

  // Build up the skia advances.
  if (glyph_count == 0)
    return PP_TRUE;
  std::vector<SkPoint> storage;
  storage.resize(glyph_count);
  SkPoint* sk_positions = &storage[0];
  for (uint32_t i = 0; i < glyph_count; i++) {
    sk_positions[i].set(x, y);
    x += SkFloatToScalar(glyph_advances[i].x);
    y += SkFloatToScalar(glyph_advances[i].y);
  }

  canvas->drawPosText(glyph_indices, glyph_count * 2, sk_positions, paint);

  return PP_TRUE;
}

int32_t PPB_Flash_Impl::Navigate(PP_Instance instance,
                                 PP_Resource request_info,
                                 const char* target,
                                 PP_Bool from_user_action) {
  EnterResourceNoLock<PPB_URLRequestInfo_API> enter(request_info, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;
  return Navigate(instance, enter.object()->GetData(), target,
                  from_user_action);
}

int32_t PPB_Flash_Impl::Navigate(PP_Instance instance,
                                 const ::ppapi::URLRequestInfoData& data,
                                 const char* target,
                                 PP_Bool from_user_action) {
  if (!target)
    return PP_ERROR_BADARGUMENT;
  return instance_->Navigate(data, target, PP_ToBool(from_user_action));
}

PP_Bool PPB_Flash_Impl::IsRectTopmost(PP_Instance instance,
                                      const PP_Rect* rect) {
  return PP_FromBool(instance_->IsRectTopmost(
      gfx::Rect(rect->point.x, rect->point.y,
                rect->size.width, rect->size.height)));
}

}  // namespace ppapi
}  // namespace webkit
