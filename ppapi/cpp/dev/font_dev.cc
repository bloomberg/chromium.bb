// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/font_dev.h"

#include <algorithm>

#include "ppapi/cpp/common.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/point.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/module_impl.h"

namespace {

DeviceFuncs<PPB_Font_Dev> font_f(PPB_FONT_DEV_INTERFACE);

}  // namespace

namespace pp {

// FontDescription_Dev ---------------------------------------------------------

FontDescription_Dev::FontDescription_Dev() {
  pp_font_description_.face = face_.pp_var();
  set_family(PP_FONTFAMILY_DEFAULT);
  set_size(0);
  set_weight(PP_FONTWEIGHT_NORMAL);
  set_italic(false);
  set_small_caps(false);
  set_letter_spacing(0);
  set_word_spacing(0);
}

FontDescription_Dev::FontDescription_Dev(const FontDescription_Dev& other) {
  set_face(other.face());
  set_family(other.family());
  set_size(other.size());
  set_weight(other.weight());
  set_italic(other.italic());
  set_small_caps(other.small_caps());
  set_letter_spacing(other.letter_spacing());
  set_word_spacing(other.word_spacing());
}

FontDescription_Dev::~FontDescription_Dev() {
}

FontDescription_Dev& FontDescription_Dev::operator=(
    const FontDescription_Dev& other) {
  FontDescription_Dev copy(other);
  swap(copy);
  return *this;
}

void FontDescription_Dev::swap(FontDescription_Dev& other) {
  // Need to fix up both the face and the pp_font_description_.face which the
  // setter does for us.
  Var temp = face();
  set_face(other.face());
  other.set_face(temp);

  std::swap(pp_font_description_.family, other.pp_font_description_.family);
  std::swap(pp_font_description_.size, other.pp_font_description_.size);
  std::swap(pp_font_description_.weight, other.pp_font_description_.weight);
  std::swap(pp_font_description_.italic, other.pp_font_description_.italic);
  std::swap(pp_font_description_.small_caps,
            other.pp_font_description_.small_caps);
  std::swap(pp_font_description_.letter_spacing,
            other.pp_font_description_.letter_spacing);
  std::swap(pp_font_description_.word_spacing,
            other.pp_font_description_.word_spacing);
}

// TextRun_Dev -----------------------------------------------------------------

TextRun_Dev::TextRun_Dev() {
  pp_text_run_.text = text_.pp_var();
  pp_text_run_.rtl = PP_FALSE;
  pp_text_run_.override_direction = PP_FALSE;
}

TextRun_Dev::TextRun_Dev(const std::string& text,
                         bool rtl,
                         bool override_direction)
    : text_(text) {
  pp_text_run_.text = text_.pp_var();
  pp_text_run_.rtl = BoolToPPBool(rtl);
  pp_text_run_.override_direction = BoolToPPBool(override_direction);
}

TextRun_Dev::TextRun_Dev(const TextRun_Dev& other) : text_(other.text_) {
  pp_text_run_.text = text_.pp_var();
  pp_text_run_.rtl = other.pp_text_run_.rtl;
  pp_text_run_.override_direction = other.pp_text_run_.override_direction;
}

TextRun_Dev::~TextRun_Dev() {
}

TextRun_Dev& TextRun_Dev::operator=(const TextRun_Dev& other) {
  TextRun_Dev copy(other);
  swap(copy);
  return *this;
}

void TextRun_Dev::swap(TextRun_Dev& other) {
  std::swap(text_, other.text_);

  // Fix up both object's pp_text_run.text to point to their text_ member.
  pp_text_run_.text = text_.pp_var();
  other.pp_text_run_.text = other.text_.pp_var();

  std::swap(pp_text_run_.rtl, other.pp_text_run_.rtl);
  std::swap(pp_text_run_.override_direction,
            other.pp_text_run_.override_direction);
}

// Font ------------------------------------------------------------------------

Font_Dev::Font_Dev(PP_Resource resource) : Resource(resource) {
}

Font_Dev::Font_Dev(const FontDescription_Dev& description) {
  if (!font_f)
    return;
  PassRefFromConstructor(font_f->Create(
      Module::Get()->pp_module(), &description.pp_font_description()));
}

Font_Dev::Font_Dev(const Font_Dev& other) : Resource(other) {
}

Font_Dev& Font_Dev::operator=(const Font_Dev& other) {
  Font_Dev copy(other);
  swap(copy);
  return *this;
}

void Font_Dev::swap(Font_Dev& other) {
  Resource::swap(other);
}

bool Font_Dev::Describe(FontDescription_Dev* description,
                        PP_FontMetrics_Dev* metrics) const {
  if (!font_f)
    return false;

  // Be careful with ownership of the |face| string. It will come back with
  // a ref of 1, which we want to assign to the |face_| member of the C++ class.
  if (!font_f->Describe(pp_resource(), &description->pp_font_description_,
                        metrics))
    return false;
  description->face_ = Var(Var::PassRef(),
                           description->pp_font_description_.face);

  return true;
}

bool Font_Dev::DrawTextAt(ImageData* dest,
                          const TextRun_Dev& text,
                          const Point& position,
                          uint32_t color,
                          const Rect& clip,
                          bool image_data_is_opaque) const {
  if (!font_f)
    return false;
  return PPBoolToBool(font_f->DrawTextAt(pp_resource(),
                                         dest->pp_resource(),
                                         &text.pp_text_run(),
                                         &position.pp_point(),
                                         color,
                                         &clip.pp_rect(),
                                         BoolToPPBool(image_data_is_opaque)));
}

int32_t Font_Dev::MeasureText(const TextRun_Dev& text) const {
  if (!font_f)
    return -1;
  return font_f->MeasureText(pp_resource(), &text.pp_text_run());
}

uint32_t Font_Dev::CharacterOffsetForPixel(const TextRun_Dev& text,
                                           int32_t pixel_position) const {
  if (!font_f)
    return 0;
  return font_f->CharacterOffsetForPixel(pp_resource(), &text.pp_text_run(),
                                         pixel_position);

}

int32_t Font_Dev::PixelOffsetForCharacter(const TextRun_Dev& text,
                                          uint32_t char_offset) const {
  if (!font_f)
    return 0;
  return font_f->PixelOffsetForCharacter(pp_resource(), &text.pp_text_run(),
                                         char_offset);
}

bool Font_Dev::DrawSimpleText(ImageData* dest,
                              const std::string& text,
                              const Point& position,
                              uint32_t color,
                              bool image_data_is_opaque) const {
  return DrawTextAt(dest, TextRun_Dev(text), position, color,
                    Rect(dest->size()), image_data_is_opaque);
}

int32_t Font_Dev::MeasureSimpleText(const std::string& text) const {
  return MeasureText(TextRun_Dev(text));
}

}  // namespace pp
