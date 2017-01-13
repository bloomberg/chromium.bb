// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/public/interfaces/ime/ime_struct_traits.h"

#include "ui/gfx/range/mojo/range_struct_traits.h"

namespace mojo {

// static
bool StructTraits<ui::mojom::CompositionUnderlineDataView,
                  ui::CompositionUnderline>::
    Read(ui::mojom::CompositionUnderlineDataView data,
         ui::CompositionUnderline* out) {
  if (data.is_null())
    return false;
  out->start_offset = data.start_offset();
  out->end_offset = data.end_offset();
  out->color = data.color();
  out->thick = data.thick();
  out->background_color = data.background_color();
  return true;
}

// static
bool StructTraits<ui::mojom::CompositionTextDataView, ui::CompositionText>::
    Read(ui::mojom::CompositionTextDataView data, ui::CompositionText* out) {
  return !data.is_null() && data.ReadText(&out->text) &&
         data.ReadUnderlines(&out->underlines) &&
         data.ReadSelection(&out->selection);
}

// static
ui::mojom::TextInputMode
EnumTraits<ui::mojom::TextInputMode, ui::TextInputMode>::ToMojom(
    ui::TextInputMode text_input_mode) {
  switch (text_input_mode) {
    case ui::TEXT_INPUT_MODE_DEFAULT:
      return ui::mojom::TextInputMode::DEFAULT;
    case ui::TEXT_INPUT_MODE_VERBATIM:
      return ui::mojom::TextInputMode::VERBATIM;
    case ui::TEXT_INPUT_MODE_LATIN:
      return ui::mojom::TextInputMode::LATIN;
    case ui::TEXT_INPUT_MODE_LATIN_NAME:
      return ui::mojom::TextInputMode::LATIN_NAME;
    case ui::TEXT_INPUT_MODE_LATIN_PROSE:
      return ui::mojom::TextInputMode::LATIN_PROSE;
    case ui::TEXT_INPUT_MODE_FULL_WIDTH_LATIN:
      return ui::mojom::TextInputMode::FULL_WIDTH_LATIN;
    case ui::TEXT_INPUT_MODE_KANA:
      return ui::mojom::TextInputMode::KANA;
    case ui::TEXT_INPUT_MODE_KANA_NAME:
      return ui::mojom::TextInputMode::KANA_NAME;
    case ui::TEXT_INPUT_MODE_KATAKANA:
      return ui::mojom::TextInputMode::KATAKANA;
    case ui::TEXT_INPUT_MODE_NUMERIC:
      return ui::mojom::TextInputMode::NUMERIC;
    case ui::TEXT_INPUT_MODE_TEL:
      return ui::mojom::TextInputMode::TEL;
    case ui::TEXT_INPUT_MODE_EMAIL:
      return ui::mojom::TextInputMode::EMAIL;
    case ui::TEXT_INPUT_MODE_URL:
      return ui::mojom::TextInputMode::URL;
  }
  NOTREACHED();
  return ui::mojom::TextInputMode::DEFAULT;
}

// static
bool EnumTraits<ui::mojom::TextInputMode, ui::TextInputMode>::FromMojom(
    ui::mojom::TextInputMode input,
    ui::TextInputMode* out) {
  switch (input) {
    case ui::mojom::TextInputMode::DEFAULT:
      *out = ui::TEXT_INPUT_MODE_DEFAULT;
      return true;
    case ui::mojom::TextInputMode::VERBATIM:
      *out = ui::TEXT_INPUT_MODE_VERBATIM;
      return true;
    case ui::mojom::TextInputMode::LATIN:
      *out = ui::TEXT_INPUT_MODE_LATIN;
      return true;
    case ui::mojom::TextInputMode::LATIN_NAME:
      *out = ui::TEXT_INPUT_MODE_LATIN_NAME;
      return true;
    case ui::mojom::TextInputMode::LATIN_PROSE:
      *out = ui::TEXT_INPUT_MODE_LATIN_PROSE;
      return true;
    case ui::mojom::TextInputMode::FULL_WIDTH_LATIN:
      *out = ui::TEXT_INPUT_MODE_FULL_WIDTH_LATIN;
      return true;
    case ui::mojom::TextInputMode::KANA:
      *out = ui::TEXT_INPUT_MODE_KANA;
      return true;
    case ui::mojom::TextInputMode::KANA_NAME:
      *out = ui::TEXT_INPUT_MODE_KANA_NAME;
      return true;
    case ui::mojom::TextInputMode::KATAKANA:
      *out = ui::TEXT_INPUT_MODE_KATAKANA;
      return true;
    case ui::mojom::TextInputMode::NUMERIC:
      *out = ui::TEXT_INPUT_MODE_NUMERIC;
      return true;
    case ui::mojom::TextInputMode::TEL:
      *out = ui::TEXT_INPUT_MODE_TEL;
      return true;
    case ui::mojom::TextInputMode::EMAIL:
      *out = ui::TEXT_INPUT_MODE_EMAIL;
      return true;
    case ui::mojom::TextInputMode::URL:
      *out = ui::TEXT_INPUT_MODE_URL;
      return true;
  }
  return false;
}

// static
ui::mojom::TextInputType
EnumTraits<ui::mojom::TextInputType, ui::TextInputType>::ToMojom(
    ui::TextInputType text_input_type) {
  switch (text_input_type) {
    case ui::TEXT_INPUT_TYPE_NONE:
      return ui::mojom::TextInputType::NONE;
    case ui::TEXT_INPUT_TYPE_TEXT:
      return ui::mojom::TextInputType::TEXT;
    case ui::TEXT_INPUT_TYPE_PASSWORD:
      return ui::mojom::TextInputType::PASSWORD;
    case ui::TEXT_INPUT_TYPE_SEARCH:
      return ui::mojom::TextInputType::SEARCH;
    case ui::TEXT_INPUT_TYPE_EMAIL:
      return ui::mojom::TextInputType::EMAIL;
    case ui::TEXT_INPUT_TYPE_NUMBER:
      return ui::mojom::TextInputType::NUMBER;
    case ui::TEXT_INPUT_TYPE_TELEPHONE:
      return ui::mojom::TextInputType::TELEPHONE;
    case ui::TEXT_INPUT_TYPE_URL:
      return ui::mojom::TextInputType::URL;
    case ui::TEXT_INPUT_TYPE_DATE:
      return ui::mojom::TextInputType::DATE;
    case ui::TEXT_INPUT_TYPE_DATE_TIME:
      return ui::mojom::TextInputType::DATETIME;
    case ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL:
      return ui::mojom::TextInputType::DATETIME_LOCAL;
    case ui::TEXT_INPUT_TYPE_MONTH:
      return ui::mojom::TextInputType::MONTH;
    case ui::TEXT_INPUT_TYPE_TIME:
      return ui::mojom::TextInputType::TIME;
    case ui::TEXT_INPUT_TYPE_WEEK:
      return ui::mojom::TextInputType::WEEK;
    case ui::TEXT_INPUT_TYPE_TEXT_AREA:
      return ui::mojom::TextInputType::TEXT_AREA;
    case ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE:
      return ui::mojom::TextInputType::CONTENT_EDITABLE;
    case ui::TEXT_INPUT_TYPE_DATE_TIME_FIELD:
      return ui::mojom::TextInputType::DATETIME_FIELD;
  }
  NOTREACHED();
  return ui::mojom::TextInputType::NONE;
}

// static
bool EnumTraits<ui::mojom::TextInputType, ui::TextInputType>::FromMojom(
    ui::mojom::TextInputType input,
    ui::TextInputType* out) {
  switch (input) {
    case ui::mojom::TextInputType::NONE:
      *out = ui::TEXT_INPUT_TYPE_NONE;
      return true;
    case ui::mojom::TextInputType::TEXT:
      *out = ui::TEXT_INPUT_TYPE_TEXT;
      return true;
    case ui::mojom::TextInputType::PASSWORD:
      *out = ui::TEXT_INPUT_TYPE_PASSWORD;
      return true;
    case ui::mojom::TextInputType::SEARCH:
      *out = ui::TEXT_INPUT_TYPE_SEARCH;
      return true;
    case ui::mojom::TextInputType::EMAIL:
      *out = ui::TEXT_INPUT_TYPE_EMAIL;
      return true;
    case ui::mojom::TextInputType::NUMBER:
      *out = ui::TEXT_INPUT_TYPE_NUMBER;
      return true;
    case ui::mojom::TextInputType::TELEPHONE:
      *out = ui::TEXT_INPUT_TYPE_TELEPHONE;
      return true;
    case ui::mojom::TextInputType::URL:
      *out = ui::TEXT_INPUT_TYPE_URL;
      return true;
    case ui::mojom::TextInputType::DATE:
      *out = ui::TEXT_INPUT_TYPE_DATE;
      return true;
    case ui::mojom::TextInputType::DATETIME:
      *out = ui::TEXT_INPUT_TYPE_DATE_TIME;
      return true;
    case ui::mojom::TextInputType::DATETIME_LOCAL:
      *out = ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL;
      return true;
    case ui::mojom::TextInputType::MONTH:
      *out = ui::TEXT_INPUT_TYPE_MONTH;
      return true;
    case ui::mojom::TextInputType::TIME:
      *out = ui::TEXT_INPUT_TYPE_TIME;
      return true;
    case ui::mojom::TextInputType::WEEK:
      *out = ui::TEXT_INPUT_TYPE_WEEK;
      return true;
    case ui::mojom::TextInputType::TEXT_AREA:
      *out = ui::TEXT_INPUT_TYPE_TEXT_AREA;
      return true;
    case ui::mojom::TextInputType::CONTENT_EDITABLE:
      *out = ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE;
      return true;
    case ui::mojom::TextInputType::DATETIME_FIELD:
      *out = ui::TEXT_INPUT_TYPE_DATE_TIME_FIELD;
      return true;
  }
  return false;
}

}  // namespace mojo
