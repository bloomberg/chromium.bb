// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/public/interfaces/ime/ime_struct_traits.h"

#include "ui/gfx/range/mojo/range_struct_traits.h"

namespace mojo {

// static
bool StructTraits<ui::mojom::CandidateWindowPropertiesDataView,
                  ui::CandidateWindow::CandidateWindowProperty>::
    Read(ui::mojom::CandidateWindowPropertiesDataView data,
         ui::CandidateWindow::CandidateWindowProperty* out) {
  if (data.is_null())
    return false;
  if (!data.ReadAuxiliaryText(&out->auxiliary_text))
    return false;
  out->page_size = data.page_size();
  out->is_vertical = data.vertical();
  out->is_auxiliary_text_visible = data.auxiliary_text_visible();
  out->cursor_position = data.cursor_position();
  out->is_cursor_visible = data.cursor_visible();
  out->show_window_at_composition =
      data.window_position() ==
      ui::mojom::CandidateWindowPosition::kComposition;
  return true;
}

// static
bool StructTraits<ui::mojom::CandidateWindowEntryDataView,
                  ui::CandidateWindow::Entry>::
    Read(ui::mojom::CandidateWindowEntryDataView data,
         ui::CandidateWindow::Entry* out) {
  return !data.is_null() && data.ReadValue(&out->value) &&
         data.ReadLabel(&out->label) && data.ReadAnnotation(&out->annotation) &&
         data.ReadDescriptionTitle(&out->description_title) &&
         data.ReadDescriptionBody(&out->description_body);
}

// static
bool StructTraits<ui::mojom::ImeTextSpanDataView, ui::ImeTextSpan>::Read(
    ui::mojom::ImeTextSpanDataView data,
    ui::ImeTextSpan* out) {
  if (data.is_null())
    return false;
  if (!data.ReadType(&out->type))
    return false;
  out->start_offset = data.start_offset();
  out->end_offset = data.end_offset();
  out->underline_color = data.underline_color();
  out->thick = data.thick();
  out->background_color = data.background_color();
  out->suggestion_highlight_color = data.suggestion_highlight_color();
  if (!data.ReadSuggestions(&out->suggestions))
    return false;
  return true;
}

// static
bool StructTraits<ui::mojom::CompositionTextDataView, ui::CompositionText>::
    Read(ui::mojom::CompositionTextDataView data, ui::CompositionText* out) {
  return !data.is_null() && data.ReadText(&out->text) &&
         data.ReadImeTextSpans(&out->ime_text_spans) &&
         data.ReadSelection(&out->selection);
}

// static
ui::mojom::ImeTextSpanType
EnumTraits<ui::mojom::ImeTextSpanType, ui::ImeTextSpan::Type>::ToMojom(
    ui::ImeTextSpan::Type ime_text_span_type) {
  switch (ime_text_span_type) {
    case ui::ImeTextSpan::Type::kComposition:
      return ui::mojom::ImeTextSpanType::kComposition;
    case ui::ImeTextSpan::Type::kSuggestion:
      return ui::mojom::ImeTextSpanType::kSuggestion;
    case ui::ImeTextSpan::Type::kMisspellingSuggestion:
      return ui::mojom::ImeTextSpanType::kMisspellingSuggestion;
  }

  NOTREACHED();
  return ui::mojom::ImeTextSpanType::kComposition;
}

// static
bool EnumTraits<ui::mojom::ImeTextSpanType, ui::ImeTextSpan::Type>::FromMojom(
    ui::mojom::ImeTextSpanType type,
    ui::ImeTextSpan::Type* out) {
  switch (type) {
    case ui::mojom::ImeTextSpanType::kComposition:
      *out = ui::ImeTextSpan::Type::kComposition;
      return true;
    case ui::mojom::ImeTextSpanType::kSuggestion:
      *out = ui::ImeTextSpan::Type::kSuggestion;
      return true;
    case ui::mojom::ImeTextSpanType::kMisspellingSuggestion:
      *out = ui::ImeTextSpan::Type::kMisspellingSuggestion;
      return true;
  }

  NOTREACHED();
  return false;
}

// static
ui::mojom::TextInputMode
EnumTraits<ui::mojom::TextInputMode, ui::TextInputMode>::ToMojom(
    ui::TextInputMode text_input_mode) {
  switch (text_input_mode) {
    case ui::TEXT_INPUT_MODE_DEFAULT:
      return ui::mojom::TextInputMode::kDefault;
    case ui::TEXT_INPUT_MODE_VERBATIM:
      return ui::mojom::TextInputMode::kVerbatim;
    case ui::TEXT_INPUT_MODE_LATIN:
      return ui::mojom::TextInputMode::kLatin;
    case ui::TEXT_INPUT_MODE_LATIN_NAME:
      return ui::mojom::TextInputMode::kLatinName;
    case ui::TEXT_INPUT_MODE_LATIN_PROSE:
      return ui::mojom::TextInputMode::kLatinProse;
    case ui::TEXT_INPUT_MODE_FULL_WIDTH_LATIN:
      return ui::mojom::TextInputMode::kFullWidthLatin;
    case ui::TEXT_INPUT_MODE_KANA:
      return ui::mojom::TextInputMode::kKana;
    case ui::TEXT_INPUT_MODE_KANA_NAME:
      return ui::mojom::TextInputMode::kKanaName;
    case ui::TEXT_INPUT_MODE_KATAKANA:
      return ui::mojom::TextInputMode::kKatakana;
    case ui::TEXT_INPUT_MODE_NUMERIC:
      return ui::mojom::TextInputMode::kNumeric;
    case ui::TEXT_INPUT_MODE_TEL:
      return ui::mojom::TextInputMode::kTel;
    case ui::TEXT_INPUT_MODE_EMAIL:
      return ui::mojom::TextInputMode::kEmail;
    case ui::TEXT_INPUT_MODE_URL:
      return ui::mojom::TextInputMode::kUrl;
  }
  NOTREACHED();
  return ui::mojom::TextInputMode::kDefault;
}

// static
bool EnumTraits<ui::mojom::TextInputMode, ui::TextInputMode>::FromMojom(
    ui::mojom::TextInputMode input,
    ui::TextInputMode* out) {
  switch (input) {
    case ui::mojom::TextInputMode::kDefault:
      *out = ui::TEXT_INPUT_MODE_DEFAULT;
      return true;
    case ui::mojom::TextInputMode::kVerbatim:
      *out = ui::TEXT_INPUT_MODE_VERBATIM;
      return true;
    case ui::mojom::TextInputMode::kLatin:
      *out = ui::TEXT_INPUT_MODE_LATIN;
      return true;
    case ui::mojom::TextInputMode::kLatinName:
      *out = ui::TEXT_INPUT_MODE_LATIN_NAME;
      return true;
    case ui::mojom::TextInputMode::kLatinProse:
      *out = ui::TEXT_INPUT_MODE_LATIN_PROSE;
      return true;
    case ui::mojom::TextInputMode::kFullWidthLatin:
      *out = ui::TEXT_INPUT_MODE_FULL_WIDTH_LATIN;
      return true;
    case ui::mojom::TextInputMode::kKana:
      *out = ui::TEXT_INPUT_MODE_KANA;
      return true;
    case ui::mojom::TextInputMode::kKanaName:
      *out = ui::TEXT_INPUT_MODE_KANA_NAME;
      return true;
    case ui::mojom::TextInputMode::kKatakana:
      *out = ui::TEXT_INPUT_MODE_KATAKANA;
      return true;
    case ui::mojom::TextInputMode::kNumeric:
      *out = ui::TEXT_INPUT_MODE_NUMERIC;
      return true;
    case ui::mojom::TextInputMode::kTel:
      *out = ui::TEXT_INPUT_MODE_TEL;
      return true;
    case ui::mojom::TextInputMode::kEmail:
      *out = ui::TEXT_INPUT_MODE_EMAIL;
      return true;
    case ui::mojom::TextInputMode::kUrl:
      *out = ui::TEXT_INPUT_MODE_URL;
      return true;
  }
  return false;
}

#define UI_TO_MOJO_TYPE_CASE(name) \
  case ui::TEXT_INPUT_TYPE_##name: \
    return ui::mojom::TextInputType::name

// static
ui::mojom::TextInputType
EnumTraits<ui::mojom::TextInputType, ui::TextInputType>::ToMojom(
    ui::TextInputType text_input_type) {
  switch (text_input_type) {
    UI_TO_MOJO_TYPE_CASE(NONE);
    UI_TO_MOJO_TYPE_CASE(TEXT);
    UI_TO_MOJO_TYPE_CASE(PASSWORD);
    UI_TO_MOJO_TYPE_CASE(SEARCH);
    UI_TO_MOJO_TYPE_CASE(EMAIL);
    UI_TO_MOJO_TYPE_CASE(NUMBER);
    UI_TO_MOJO_TYPE_CASE(TELEPHONE);
    UI_TO_MOJO_TYPE_CASE(URL);
    UI_TO_MOJO_TYPE_CASE(DATE);
    UI_TO_MOJO_TYPE_CASE(DATE_TIME);
    UI_TO_MOJO_TYPE_CASE(DATE_TIME_LOCAL);
    UI_TO_MOJO_TYPE_CASE(MONTH);
    UI_TO_MOJO_TYPE_CASE(TIME);
    UI_TO_MOJO_TYPE_CASE(WEEK);
    UI_TO_MOJO_TYPE_CASE(TEXT_AREA);
    UI_TO_MOJO_TYPE_CASE(CONTENT_EDITABLE);
    UI_TO_MOJO_TYPE_CASE(DATE_TIME_FIELD);
  }
  NOTREACHED();
  return ui::mojom::TextInputType::NONE;
}

#undef UI_TO_MOJO_TYPE_CASE

#define MOJO_TO_UI_TYPE_CASE(name)     \
  case ui::mojom::TextInputType::name: \
    *out = ui::TEXT_INPUT_TYPE_##name; \
    return true;

// static
bool EnumTraits<ui::mojom::TextInputType, ui::TextInputType>::FromMojom(
    ui::mojom::TextInputType input,
    ui::TextInputType* out) {
  switch (input) {
    MOJO_TO_UI_TYPE_CASE(NONE);
    MOJO_TO_UI_TYPE_CASE(TEXT);
    MOJO_TO_UI_TYPE_CASE(PASSWORD);
    MOJO_TO_UI_TYPE_CASE(SEARCH);
    MOJO_TO_UI_TYPE_CASE(EMAIL);
    MOJO_TO_UI_TYPE_CASE(NUMBER);
    MOJO_TO_UI_TYPE_CASE(TELEPHONE);
    MOJO_TO_UI_TYPE_CASE(URL);
    MOJO_TO_UI_TYPE_CASE(DATE);
    MOJO_TO_UI_TYPE_CASE(DATE_TIME);
    MOJO_TO_UI_TYPE_CASE(DATE_TIME_LOCAL);
    MOJO_TO_UI_TYPE_CASE(MONTH);
    MOJO_TO_UI_TYPE_CASE(TIME);
    MOJO_TO_UI_TYPE_CASE(WEEK);
    MOJO_TO_UI_TYPE_CASE(TEXT_AREA);
    MOJO_TO_UI_TYPE_CASE(CONTENT_EDITABLE);
    MOJO_TO_UI_TYPE_CASE(DATE_TIME_FIELD);
  }
#undef MOJO_TO_UI_TYPE_CASE
  return false;
}

}  // namespace mojo
