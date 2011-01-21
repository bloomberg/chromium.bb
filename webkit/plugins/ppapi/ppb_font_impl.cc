// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_font_impl.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "ppapi/c/dev/ppb_font_dev.h"
#include "ppapi/c/pp_rect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFont.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFontDescription.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFloatPoint.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFloatRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextRun.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_image_data_impl.h"
#include "webkit/plugins/ppapi/string.h"
#include "webkit/plugins/ppapi/var.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebFloatPoint;
using WebKit::WebFloatRect;
using WebKit::WebFont;
using WebKit::WebFontDescription;
using WebKit::WebRect;
using WebKit::WebTextRun;

namespace webkit {
namespace ppapi {

namespace {

bool IsPPFontDescriptionValid(const PP_FontDescription_Dev& desc) {
  // Check validity of UTF-8.
  if (desc.face.type != PP_VARTYPE_STRING &&
      desc.face.type != PP_VARTYPE_UNDEFINED)
    return false;

  // Check enum ranges.
  if (static_cast<int>(desc.family) < PP_FONTFAMILY_DEFAULT ||
      static_cast<int>(desc.family) > PP_FONTFAMILY_MONOSPACE)
    return false;
  if (static_cast<int>(desc.weight) < PP_FONTWEIGHT_100 ||
      static_cast<int>(desc.weight) > PP_FONTWEIGHT_900)
    return false;

  // Check for excessive sizes which may cause layout to get confused.
  if (desc.size > 200)
    return false;

  return true;
}

// The PP_* version lacks "None", so is just one value shifted from the
// WebFontDescription version. These values are checked in
// PPFontDescToWebFontDesc to make sure the conversion is correct. This is a
// macro so it can also be used in the COMPILE_ASSERTS.
#define PP_FONTFAMILY_TO_WEB_FONTFAMILY(f) \
  static_cast<WebFontDescription::GenericFamily>(f + 1)

// Assumes the given PP_FontDescription has been validated.
WebFontDescription PPFontDescToWebFontDesc(const PP_FontDescription_Dev& font) {
  // Verify that the enums match so we can just static cast.
  COMPILE_ASSERT(static_cast<int>(WebFontDescription::Weight100) ==
                 static_cast<int>(PP_FONTWEIGHT_100),
                 FontWeight100);
  COMPILE_ASSERT(static_cast<int>(WebFontDescription::Weight900) ==
                 static_cast<int>(PP_FONTWEIGHT_900),
                 FontWeight900);
  COMPILE_ASSERT(WebFontDescription::GenericFamilyStandard ==
                 PP_FONTFAMILY_TO_WEB_FONTFAMILY(PP_FONTFAMILY_DEFAULT),
                 StandardFamily);
  COMPILE_ASSERT(WebFontDescription::GenericFamilySerif ==
                 PP_FONTFAMILY_TO_WEB_FONTFAMILY(PP_FONTFAMILY_SERIF),
                 SerifFamily);
  COMPILE_ASSERT(WebFontDescription::GenericFamilySansSerif ==
                 PP_FONTFAMILY_TO_WEB_FONTFAMILY(PP_FONTFAMILY_SANSSERIF),
                 SansSerifFamily);
  COMPILE_ASSERT(WebFontDescription::GenericFamilyMonospace ==
                 PP_FONTFAMILY_TO_WEB_FONTFAMILY(PP_FONTFAMILY_MONOSPACE),
                 MonospaceFamily);

  WebFontDescription result;
  scoped_refptr<StringVar> face_name(StringVar::FromPPVar(font.face));
  if (face_name)
    result.family = UTF8ToUTF16(face_name->value());
  result.genericFamily = PP_FONTFAMILY_TO_WEB_FONTFAMILY(font.family);
  result.size = static_cast<float>(font.size);
  result.italic = PPBoolToBool(font.italic);
  result.smallCaps = PPBoolToBool(font.small_caps);
  result.weight = static_cast<WebFontDescription::Weight>(font.weight);
  result.letterSpacing = static_cast<short>(font.letter_spacing);
  result.wordSpacing = static_cast<short>(font.word_spacing);
  return result;
}

// Converts the given PP_TextRun to a WebTextRun, returning true on success.
// False means the input was invalid.
bool PPTextRunToWebTextRun(const PP_TextRun_Dev* run, WebTextRun* output) {
  scoped_refptr<StringVar> text_string(StringVar::FromPPVar(run->text));
  if (!text_string)
    return false;
  *output = WebTextRun(UTF8ToUTF16(text_string->value()),
                       PPBoolToBool(run->rtl),
                       PPBoolToBool(run->override_direction));
  return true;
}

PP_Resource Create(PP_Instance instance_id,
                   const PP_FontDescription_Dev* description) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return 0;

  if (!IsPPFontDescriptionValid(*description))
    return 0;

  scoped_refptr<PPB_Font_Impl> font(new PPB_Font_Impl(instance, *description));
  return font->GetReference();
}

PP_Bool IsFont(PP_Resource resource) {
  return BoolToPPBool(!!Resource::GetAs<PPB_Font_Impl>(resource).get());
}

PP_Bool Describe(PP_Resource font_id,
                 PP_FontDescription_Dev* description,
                 PP_FontMetrics_Dev* metrics) {
  scoped_refptr<PPB_Font_Impl> font(Resource::GetAs<PPB_Font_Impl>(font_id));
  if (!font.get())
    return PP_FALSE;
  return BoolToPPBool(font->Describe(description, metrics));
}

PP_Bool DrawTextAt(PP_Resource font_id,
                   PP_Resource image_data,
                   const PP_TextRun_Dev* text,
                   const PP_Point* position,
                   uint32_t color,
                   const PP_Rect* clip,
                   PP_Bool image_data_is_opaque) {
  scoped_refptr<PPB_Font_Impl> font(Resource::GetAs<PPB_Font_Impl>(font_id));
  if (!font.get())
    return PP_FALSE;
  return BoolToPPBool(font->DrawTextAt(image_data, text, position, color, clip,
                                       PPBoolToBool(image_data_is_opaque)));
}

int32_t MeasureText(PP_Resource font_id, const PP_TextRun_Dev* text) {
  scoped_refptr<PPB_Font_Impl> font(Resource::GetAs<PPB_Font_Impl>(font_id));
  if (!font.get())
    return -1;
  return font->MeasureText(text);
}

uint32_t CharacterOffsetForPixel(PP_Resource font_id,
                                 const PP_TextRun_Dev* text,
                                 int32_t pixel_position) {
  scoped_refptr<PPB_Font_Impl> font(Resource::GetAs<PPB_Font_Impl>(font_id));
  if (!font.get())
    return -1;
  return font->CharacterOffsetForPixel(text, pixel_position);
}

int32_t PixelOffsetForCharacter(PP_Resource font_id,
                                const PP_TextRun_Dev* text,
                                uint32_t char_offset) {
  scoped_refptr<PPB_Font_Impl> font(Resource::GetAs<PPB_Font_Impl>(font_id));
  if (!font.get())
    return false;
  return font->PixelOffsetForCharacter(text, char_offset);
}

const PPB_Font_Dev ppb_font = {
  &Create,
  &IsFont,
  &Describe,
  &DrawTextAt,
  &MeasureText,
  &CharacterOffsetForPixel,
  &PixelOffsetForCharacter
};

}  // namespace

PPB_Font_Impl::PPB_Font_Impl(PluginInstance* instance,
                             const PP_FontDescription_Dev& desc)
    : Resource(instance) {
  WebFontDescription web_font_desc = PPFontDescToWebFontDesc(desc);
  font_.reset(WebFont::create(web_font_desc));
}

PPB_Font_Impl::~PPB_Font_Impl() {
}

// static
const PPB_Font_Dev* PPB_Font_Impl::GetInterface() {
  return &ppb_font;
}

PPB_Font_Impl* PPB_Font_Impl::AsPPB_Font_Impl() {
  return this;
}

bool PPB_Font_Impl::Describe(PP_FontDescription_Dev* description,
                             PP_FontMetrics_Dev* metrics) {
  if (description->face.type != PP_VARTYPE_UNDEFINED)
    return false;

  WebFontDescription web_desc = font_->fontDescription();

  // While converting the other way in PPFontDescToWebFontDesc we validated
  // that the enums can be casted.
  description->face = StringVar::StringToPPVar(instance()->module(),
                                               UTF16ToUTF8(web_desc.family));
  description->family = static_cast<PP_FontFamily_Dev>(web_desc.genericFamily);
  description->size = static_cast<uint32_t>(web_desc.size);
  description->weight = static_cast<PP_FontWeight_Dev>(web_desc.weight);
  description->italic = BoolToPPBool(web_desc.italic);
  description->small_caps = BoolToPPBool(web_desc.smallCaps);

  metrics->height = font_->height();
  metrics->ascent = font_->ascent();
  metrics->descent = font_->descent();
  metrics->line_spacing = font_->lineSpacing();
  metrics->x_height = static_cast<int32_t>(font_->xHeight());

  return true;
}

bool PPB_Font_Impl::DrawTextAt(PP_Resource image_data,
                               const PP_TextRun_Dev* text,
                               const PP_Point* position,
                               uint32_t color,
                               const PP_Rect* clip,
                               bool image_data_is_opaque) {
  WebTextRun run;
  if (!PPTextRunToWebTextRun(text, &run))
    return false;

  // Get and map the image data we're painting to.
  scoped_refptr<PPB_ImageData_Impl> image_resource(
      Resource::GetAs<PPB_ImageData_Impl>(image_data));
  if (!image_resource.get())
    return false;
  ImageDataAutoMapper mapper(image_resource);
  if (!mapper.is_valid())
    return false;

  // Convert position and clip.
  WebFloatPoint web_position(static_cast<float>(position->x),
                             static_cast<float>(position->y));
  WebRect web_clip;
  if (!clip) {
    // Use entire canvas.
    web_clip = WebRect(0, 0, image_resource->width(), image_resource->height());
  } else {
    web_clip = WebRect(clip->point.x, clip->point.y,
                       clip->size.width, clip->size.height);
  }

  font_->drawText(webkit_glue::ToWebCanvas(image_resource->mapped_canvas()),
                  run, web_position, color, web_clip, image_data_is_opaque);
  return true;
}

int32_t PPB_Font_Impl::MeasureText(const PP_TextRun_Dev* text) {
  WebTextRun run;
  if (!PPTextRunToWebTextRun(text, &run))
    return -1;
  return font_->calculateWidth(run);
}

uint32_t PPB_Font_Impl::CharacterOffsetForPixel(const PP_TextRun_Dev* text,
                                                int32_t pixel_position) {
  WebTextRun run;
  if (!PPTextRunToWebTextRun(text, &run))
    return -1;

  return static_cast<uint32_t>(font_->offsetForPosition(
      run, static_cast<float>(pixel_position)));
}

int32_t PPB_Font_Impl::PixelOffsetForCharacter(const PP_TextRun_Dev* text,
                                               uint32_t char_offset) {
  WebTextRun run;
  if (!PPTextRunToWebTextRun(text, &run))
    return -1;
  if (char_offset >= run.text.length())
    return -1;

  WebFloatRect rect = font_->selectionRectForText(
      run, WebFloatPoint(0.0f, 0.0f), font_->height(), 0, char_offset);
  return static_cast<int>(rect.width);
}

}  // namespace ppapi
}  // namespace webkit

