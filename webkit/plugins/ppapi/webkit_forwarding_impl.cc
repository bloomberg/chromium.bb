// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/webkit_forwarding_impl.h"

#include <string>

#include "base/debug/trace_event.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/utf_string_conversions.h"
#include "ppapi/c/dev/ppb_font_dev.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/shared_impl/ppapi_preferences.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebCanvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFont.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFontDescription.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebFloatRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextRun.h"
#include "webkit/glue/webkit_glue.h"

using ::ppapi::WebKitForwarding;
using WebKit::WebCanvas;
using WebKit::WebFloatPoint;
using WebKit::WebFloatRect;
using WebKit::WebFont;
using WebKit::WebFontDescription;
using WebKit::WebRect;
using WebKit::WebTextRun;

namespace webkit {
namespace ppapi {

namespace {

// The PP_* version lacks "None", so is just one value shifted from the
// WebFontDescription version. These values are checked in
// PPFontDescToWebFontDesc to make sure the conversion is correct. This is a
// macro so it can also be used in the COMPILE_ASSERTS.
#define PP_FONTFAMILY_TO_WEB_FONTFAMILY(f) \
  static_cast<WebFontDescription::GenericFamily>(f + 1)

// Assumes the given PP_FontDescription has been validated.
WebFontDescription PPFontDescToWebFontDesc(const PP_FontDescription_Dev& font,
                                           const std::string& face,
                                           const ::ppapi::Preferences& prefs) {
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
  string16 resolved_family;
  if (face.empty()) {
    // Resolve the generic family.
    switch (font.family) {
      case PP_FONTFAMILY_SERIF:
        resolved_family = prefs.serif_font_family;
        break;
      case PP_FONTFAMILY_SANSSERIF:
        resolved_family = prefs.sans_serif_font_family;
        break;
      case PP_FONTFAMILY_MONOSPACE:
        resolved_family = prefs.fixed_font_family;
        break;
      case PP_FONTFAMILY_DEFAULT:
      default:
        resolved_family = prefs.standard_font_family;
        break;
    }
  } else {
    // Use the exact font.
    resolved_family = UTF8ToUTF16(face);
  }
  result.family = resolved_family;

  result.genericFamily = PP_FONTFAMILY_TO_WEB_FONTFAMILY(font.family);

  if (font.size == 0) {
    // Resolve the default font size, using the resolved family to see if
    // we should use the fixed or regular font size. It's difficult at this
    // level to detect if the requested font is fixed width, so we only apply
    // the alternate font size to the default fixed font family.
    if (StringToLowerASCII(resolved_family) ==
        StringToLowerASCII(prefs.fixed_font_family))
      result.size = static_cast<float>(prefs.default_fixed_font_size);
    else
      result.size = static_cast<float>(prefs.default_font_size);
  } else {
    // Use the exact size.
    result.size = static_cast<float>(font.size);
  }

  result.italic = font.italic != PP_FALSE;
  result.smallCaps = font.small_caps != PP_FALSE;
  result.weight = static_cast<WebFontDescription::Weight>(font.weight);
  result.letterSpacing = static_cast<short>(font.letter_spacing);
  result.wordSpacing = static_cast<short>(font.word_spacing);
  return result;
}

WebTextRun TextRunToWebTextRun(const WebKitForwarding::Font::TextRun& run) {
  return WebTextRun(UTF8ToUTF16(run.text),
                    run.rtl != PP_FALSE,
                    run.override_direction != PP_FALSE);
}

// FontImpl --------------------------------------------------------------------

class FontImpl : public WebKitForwarding::Font {
 public:
  FontImpl(const PP_FontDescription_Dev& desc,
           const std::string& desc_face,
           const ::ppapi::Preferences& prefs);
  virtual ~FontImpl();

  virtual void Describe(base::WaitableEvent* event,
                        PP_FontDescription_Dev* description,
                        std::string* face,
                        PP_FontMetrics_Dev* metrics,
                        PP_Bool* result) OVERRIDE;
  virtual void DrawTextAt(base::WaitableEvent* event,
                          const DrawTextParams& params) OVERRIDE;
  virtual void MeasureText(base::WaitableEvent* event,
                           const TextRun& text,
                           int32_t* result) OVERRIDE;
  virtual void CharacterOffsetForPixel(base::WaitableEvent* event,
                                       const TextRun& text,
                                       int32_t pixel_position,
                                       uint32_t* result) OVERRIDE;
  virtual void PixelOffsetForCharacter(base::WaitableEvent* event,
                                       const TextRun& text,
                                       uint32_t char_offset,
                                       int32_t* result) OVERRIDE;

 private:
  scoped_ptr<WebFont> font_;

  DISALLOW_COPY_AND_ASSIGN(FontImpl);
};

FontImpl::FontImpl(const PP_FontDescription_Dev& desc,
                   const std::string& desc_face,
                   const ::ppapi::Preferences& prefs) {
  WebFontDescription web_font_desc = PPFontDescToWebFontDesc(desc, desc_face,
                                                             prefs);
  font_.reset(WebFont::create(web_font_desc));
}

FontImpl::~FontImpl() {
}

void FontImpl::Describe(base::WaitableEvent* event,
                        PP_FontDescription_Dev* description,
                        std::string* face,
                        PP_FontMetrics_Dev* metrics,
                        PP_Bool* result) {
  TRACE_EVENT0("ppapi WebKit thread", "FontImpl::Describe");
  if (description->face.type != PP_VARTYPE_UNDEFINED) {
    *result = PP_FALSE;
  } else {
    WebFontDescription web_desc = font_->fontDescription();

    // While converting the other way in PPFontDescToWebFontDesc we validated
    // that the enums can be casted.
    description->face = PP_MakeUndefined();
    description->family =
        static_cast<PP_FontFamily_Dev>(web_desc.genericFamily);
    description->size = static_cast<uint32_t>(web_desc.size);
    description->weight = static_cast<PP_FontWeight_Dev>(web_desc.weight);
    description->italic = web_desc.italic ? PP_TRUE : PP_FALSE;
    description->small_caps = web_desc.smallCaps ? PP_TRUE : PP_FALSE;
    description->letter_spacing = static_cast<int32_t>(web_desc.letterSpacing);
    description->word_spacing = static_cast<int32_t>(web_desc.wordSpacing);

    *face = UTF16ToUTF8(web_desc.family);

    metrics->height = font_->height();
    metrics->ascent = font_->ascent();
    metrics->descent = font_->descent();
    metrics->line_spacing = font_->lineSpacing();
    metrics->x_height = static_cast<int32_t>(font_->xHeight());

    *result = PP_TRUE;
  }
  if (event)
    event->Signal();
}

void FontImpl::DrawTextAt(base::WaitableEvent* event,
                          const DrawTextParams& params) {
  TRACE_EVENT0("ppapi WebKit thread", "FontImpl::DrawTextAt");
  WebTextRun run = TextRunToWebTextRun(params.text);

  // Convert position and clip.
  WebFloatPoint web_position(static_cast<float>(params.position->x),
                             static_cast<float>(params.position->y));
  WebRect web_clip;
  if (!params.clip) {
    // Use entire canvas. SkCanvas doesn't have a size on it, so we just use
    // the current clip bounds.
    SkRect skclip;
    params.destination->getClipBounds(&skclip);
    web_clip = WebRect(skclip.fLeft, skclip.fTop, skclip.fRight - skclip.fLeft,
                       skclip.fBottom - skclip.fTop);
  } else {
    web_clip = WebRect(params.clip->point.x, params.clip->point.y,
                       params.clip->size.width, params.clip->size.height);
  }

  font_->drawText(webkit_glue::ToWebCanvas(params.destination), run,
                  web_position, params.color, web_clip,
                  params.image_data_is_opaque == PP_TRUE);
  if (event)
    event->Signal();
}

void FontImpl::MeasureText(base::WaitableEvent* event,
                           const TextRun& text, int32_t* result) {
  TRACE_EVENT0("ppapi WebKit thread", "FontImpl::MeasureText");
  *result = font_->calculateWidth(TextRunToWebTextRun(text));
  if (event)
    event->Signal();
}

void FontImpl::CharacterOffsetForPixel(base::WaitableEvent* event,
                                       const TextRun& text,
                                       int32_t pixel_position,
                                       uint32_t* result) {
  TRACE_EVENT0("ppapi WebKit thread", "FontImpl::CharacterOffsetForPixel");
  *result = static_cast<uint32_t>(font_->offsetForPosition(
      TextRunToWebTextRun(text), static_cast<float>(pixel_position)));
  if (event)
    event->Signal();
}

void FontImpl::PixelOffsetForCharacter(base::WaitableEvent* event,
                                       const TextRun& text,
                                       uint32_t char_offset,
                                       int32_t* result) {
  TRACE_EVENT0("ppapi WebKit thread", "FontImpl::PixelOffsetForCharacter");
  WebTextRun run = TextRunToWebTextRun(text);
  if (char_offset >= run.text.length()) {
    *result = -1;
  } else {
    WebFloatRect rect = font_->selectionRectForText(
        run, WebFloatPoint(0.0f, 0.0f), font_->height(), 0, char_offset);
    *result = static_cast<int>(rect.width);
  }
  if (event)
    event->Signal();
}

}  // namespace

// WebKitForwardingImpl --------------------------------------------------------

WebKitForwardingImpl::WebKitForwardingImpl() {
}

WebKitForwardingImpl::~WebKitForwardingImpl() {
}

void WebKitForwardingImpl::CreateFontForwarding(
    base::WaitableEvent* event,
    const PP_FontDescription_Dev& desc,
    const std::string& desc_face,
    const ::ppapi::Preferences& prefs,
    Font** result) {
  TRACE_EVENT0("ppapi WebKit thread",
               "WebKitForwardingImpl::CreateFontForwarding");
  *result = new FontImpl(desc, desc_face, prefs);
  if (event)
    event->Signal();
}

}  // namespace ppapi
}  // namespace webkit
