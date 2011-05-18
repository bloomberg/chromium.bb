// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_font_impl.h"

#include "ppapi/c/dev/ppb_font_dev.h"
#include "ppapi/thunk/thunk.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_image_data_impl.h"
#include "webkit/plugins/ppapi/string.h"
#include "webkit/plugins/ppapi/var.h"

using ::ppapi::WebKitForwarding;

namespace webkit {
namespace ppapi {

namespace {

// Converts the given PP_TextRun to a TextRun, returning true on success.
// False means the input was invalid.
bool PPTextRunToTextRun(const PP_TextRun_Dev* run,
                        WebKitForwarding::Font::TextRun* output) {
  scoped_refptr<StringVar> text_string(StringVar::FromPPVar(run->text));
  if (!text_string)
    return false;

  output->text = text_string->value();
  output->rtl = PPBoolToBool(run->rtl);
  output->override_direction = PPBoolToBool(run->override_direction);
  return true;
}

}  // namespace

PPB_Font_Impl::PPB_Font_Impl(PluginInstance* instance,
                             const PP_FontDescription_Dev& desc)
    : Resource(instance) {
  scoped_refptr<StringVar> face_name(StringVar::FromPPVar(desc.face));

  WebKitForwarding::Font* result = NULL;
  instance->module()->GetWebKitForwarding()->CreateFontForwarding(
      NULL, desc, face_name ? face_name->value() : std::string(), &result);
  font_forwarding_.reset(result);
}

PPB_Font_Impl::~PPB_Font_Impl() {
}

::ppapi::thunk::PPB_Font_API* PPB_Font_Impl::AsFont_API() {
  return this;
}

PPB_Font_Impl* PPB_Font_Impl::AsPPB_Font_Impl() {
  return this;
}

PP_Bool PPB_Font_Impl::Describe(PP_FontDescription_Dev* description,
                                PP_FontMetrics_Dev* metrics) {
  std::string face;
  PP_Bool result = PP_FALSE;
  font_forwarding_->Describe(NULL, description, &face, metrics, &result);
  if (!result)
    return PP_FALSE;

  // Convert the string.
  description->face = StringVar::StringToPPVar(instance()->module(), face);
  return PP_TRUE;
}

PP_Bool PPB_Font_Impl::DrawTextAt(PP_Resource image_data,
                                  const PP_TextRun_Dev* text,
                                  const PP_Point* position,
                                  uint32_t color,
                                  const PP_Rect* clip,
                                  PP_Bool image_data_is_opaque) {
  // Get and map the image data we're painting to.
  scoped_refptr<PPB_ImageData_Impl> image_resource(
      Resource::GetAs<PPB_ImageData_Impl>(image_data));
  if (!image_resource.get())
    return PP_FALSE;
  ImageDataAutoMapper mapper(image_resource);
  if (!mapper.is_valid())
    return PP_FALSE;

  WebKitForwarding::Font::TextRun run;
  if (!PPTextRunToTextRun(text, &run))
    return PP_FALSE;

  font_forwarding_->DrawTextAt(NULL, WebKitForwarding::Font::DrawTextParams(
      image_resource->mapped_canvas(), run, position,
      color, clip, image_data_is_opaque));
  return PP_TRUE;
}

int32_t PPB_Font_Impl::MeasureText(const PP_TextRun_Dev* text) {
  int32_t result = -1;
  WebKitForwarding::Font::TextRun run;
  if (PPTextRunToTextRun(text, &run))
    font_forwarding_->MeasureText(NULL, run, &result);
  return result;
}

uint32_t PPB_Font_Impl::CharacterOffsetForPixel(const PP_TextRun_Dev* text,
                                                int32_t pixel_position) {
  uint32_t result = -1;
  WebKitForwarding::Font::TextRun run;
  if (PPTextRunToTextRun(text, &run)) {
    font_forwarding_->CharacterOffsetForPixel(NULL, run, pixel_position,
                                              &result);
  }
  return result;
}

int32_t PPB_Font_Impl::PixelOffsetForCharacter(const PP_TextRun_Dev* text,
                                               uint32_t char_offset) {
  int32_t result = -1;
  WebKitForwarding::Font::TextRun run;
  if (PPTextRunToTextRun(text, &run)) {
    font_forwarding_->PixelOffsetForCharacter(NULL, run, char_offset, &result);
  }
  return result;
}

PPB_Font_FunctionImpl::PPB_Font_FunctionImpl() {
}

PPB_Font_FunctionImpl::~PPB_Font_FunctionImpl() {
}

::ppapi::thunk::PPB_Font_FunctionAPI*
PPB_Font_FunctionImpl::AsFont_FunctionAPI() {
  return this;
}

PP_Var PPB_Font_FunctionImpl::GetFontFamilies(PP_Instance instance) {
  return PP_MakeUndefined();
}

}  // namespace ppapi
}  // namespace webkit

