// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_flash_impl.h"

#include <string>
#include <vector>

#include "base/message_loop.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "ppapi/c/dev/ppb_font_dev.h"
#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/shared_impl/time_conversion.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_image_data_api.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "third_party/skia/include/core/SkTemplates.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/host_globals.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_url_request_info_impl.h"
#include "webkit/plugins/ppapi/resource_helper.h"
#include "webkit/plugins/ppapi/ppb_image_data_impl.h"

using ppapi::PPTimeToTime;
using ppapi::StringVar;
using ppapi::thunk::EnterResource;
using ppapi::thunk::PPB_ImageData_API;
using ppapi::thunk::PPB_URLRequestInfo_API;

namespace webkit {
namespace ppapi {

namespace {

void SetInstanceAlwaysOnTop(PP_Instance pp_instance, PP_Bool on_top) {
  PluginInstance* instance = HostGlobals::Get()->GetInstance(pp_instance);
  if (!instance)
    return;
  instance->set_always_on_top(PPBoolToBool(on_top));
}


PP_Bool DrawGlyphs(PP_Instance,
                   PP_Resource pp_image_data,
                   const PP_FontDescription_Dev* font_desc,
                   uint32_t color,
                   const PP_Point* position,
                   const PP_Rect* clip,
                   const float transformation[3][3],
                   PP_Bool allow_subpixel_aa,
                   uint32_t glyph_count,
                   const uint16_t glyph_indices[],
                   const PP_Point glyph_advances[]) {
  EnterResource<PPB_ImageData_API> enter(pp_image_data, true);
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
  if (font_desc->weight >= PP_FONTWEIGHT_BOLD)
    style |= SkTypeface::kBold;
  if (font_desc->italic)
    style |= SkTypeface::kItalic;
  SkTypeface* typeface =
      SkTypeface::CreateFromName(face_name->value().c_str(),
                                 static_cast<SkTypeface::Style>(style));
  if (!typeface)
    return PP_FALSE;
  SkAutoUnref aur(typeface);

  // Set up the canvas.
  SkCanvas* canvas = image_resource->mapped_canvas();
  SkAutoCanvasRestore acr(canvas, true);

  // Clip is applied in pixels before the transform.
  SkRect clip_rect = { clip->point.x, clip->point.y,
                       clip->point.x + clip->size.width,
                       clip->point.y + clip->size.height };
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
  paint.setTypeface(typeface);  // Takes a ref and manages lifetime.
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

PP_Bool DrawGlyphs11(PP_Instance instance,
                     PP_Resource pp_image_data,
                     const PP_FontDescription_Dev* font_desc,
                     uint32_t color,
                     PP_Point position,
                     PP_Rect clip,
                     const float transformation[3][3],
                     uint32_t glyph_count,
                     const uint16_t glyph_indices[],
                     const PP_Point glyph_advances[]) {
  return DrawGlyphs(instance, pp_image_data, font_desc, color, &position,
                    &clip, transformation, PP_TRUE, glyph_count,
                    glyph_indices, glyph_advances);
}

PP_Var GetProxyForURL(PP_Instance pp_instance, const char* url) {
  PluginInstance* instance = HostGlobals::Get()->GetInstance(pp_instance);
  if (!instance)
    return PP_MakeUndefined();

  GURL gurl(url);
  if (!gurl.is_valid())
    return PP_MakeUndefined();

  std::string proxy_host = instance->delegate()->ResolveProxy(gurl);
  if (proxy_host.empty())
    return PP_MakeUndefined();  // No proxy.
  return StringVar::StringToPPVar(proxy_host);
}

int32_t Navigate(PP_Resource request_id,
                 const char* target,
                 PP_Bool from_user_action) {
  EnterResource<PPB_URLRequestInfo_API> enter(request_id, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;
  PPB_URLRequestInfo_Impl* request =
      static_cast<PPB_URLRequestInfo_Impl*>(enter.object());

  if (!target)
    return PP_ERROR_BADARGUMENT;

  PluginInstance* plugin_instance = ResourceHelper::GetPluginInstance(request);
  if (!plugin_instance)
    return PP_ERROR_FAILED;
  return plugin_instance->Navigate(request, target,
                                   PP_ToBool(from_user_action));
}

int32_t Navigate11(PP_Resource request_id,
                   const char* target,
                   bool from_user_action) {
  return Navigate(request_id, target, PP_FromBool(from_user_action));
}

void RunMessageLoop(PP_Instance instance) {
  bool old_state = MessageLoop::current()->NestableTasksAllowed();
  MessageLoop::current()->SetNestableTasksAllowed(true);
  MessageLoop::current()->Run();
  MessageLoop::current()->SetNestableTasksAllowed(old_state);
}

void QuitMessageLoop(PP_Instance instance) {
  MessageLoop::current()->QuitNow();
}

double GetLocalTimeZoneOffset(PP_Instance pp_instance, PP_Time t) {
  PluginInstance* instance = HostGlobals::Get()->GetInstance(pp_instance);
  if (!instance)
    return 0.0;

  // Evil hack. The time code handles exact "0" values as special, and produces
  // a "null" Time object. This will represent some date hundreds of years ago
  // and will give us funny results at 1970 (there are some tests where this
  // comes up, but it shouldn't happen in real life). To work around this
  // special handling, we just need to give it some nonzero value.
  if (t == 0.0)
    t = 0.0000000001;

  // We can't do the conversion here because on Linux, the localtime calls
  // require filesystem access prohibited by the sandbox.
  return instance->delegate()->GetLocalTimeZoneOffset(PPTimeToTime(t));
}

PP_Var GetCommandLineArgs(PP_Module pp_module) {
  PluginModule* module = HostGlobals::Get()->GetModule(pp_module);
  if (!module)
    return PP_MakeUndefined();

  PluginInstance* instance = module->GetSomeInstance();
  if (!instance)
    return PP_MakeUndefined();

  std::string args = instance->delegate()->GetFlashCommandLineArgs();
  return StringVar::StringToPPVar(args);
}

void PreLoadFontWin(const void* logfontw) {
  // Not implemented in-process.
}

const PPB_Flash_11 ppb_flash_11 = {
  &SetInstanceAlwaysOnTop,
  &DrawGlyphs11,
  &GetProxyForURL,
  &Navigate11,
  &RunMessageLoop,
  &QuitMessageLoop,
  &GetLocalTimeZoneOffset,
  &GetCommandLineArgs
};

const PPB_Flash ppb_flash_12 = {
  &SetInstanceAlwaysOnTop,
  &DrawGlyphs,
  &GetProxyForURL,
  &Navigate,
  &RunMessageLoop,
  &QuitMessageLoop,
  &GetLocalTimeZoneOffset,
  &GetCommandLineArgs,
  &PreLoadFontWin
};

}  // namespace

// static
const PPB_Flash_11* PPB_Flash_Impl::GetInterface11() {
  return &ppb_flash_11;
}

// static
const PPB_Flash_12_0* PPB_Flash_Impl::GetInterface12_0() {
  return &ppb_flash_12;
}

}  // namespace ppapi
}  // namespace webkit
