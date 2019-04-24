// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/public/mojom/ime/ime_struct_traits.h"

#include "mojo/public/cpp/base/string16_mojom_traits.h"
#include "ui/gfx/range/mojo/range_struct_traits.h"

namespace mojo {

// static
bool StructTraits<ws::mojom::CandidateWindowPropertiesDataView,
                  ui::CandidateWindow::CandidateWindowProperty>::
    Read(ws::mojom::CandidateWindowPropertiesDataView data,
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
      ws::mojom::CandidateWindowPosition::kComposition;
  return true;
}

// static
bool StructTraits<ws::mojom::CandidateWindowEntryDataView,
                  ui::CandidateWindow::Entry>::
    Read(ws::mojom::CandidateWindowEntryDataView data,
         ui::CandidateWindow::Entry* out) {
  return !data.is_null() && data.ReadValue(&out->value) &&
         data.ReadLabel(&out->label) && data.ReadAnnotation(&out->annotation) &&
         data.ReadDescriptionTitle(&out->description_title) &&
         data.ReadDescriptionBody(&out->description_body);
}

// static
bool StructTraits<ws::mojom::ImeTextSpanDataView, ui::ImeTextSpan>::Read(
    ws::mojom::ImeTextSpanDataView data,
    ui::ImeTextSpan* out) {
  if (data.is_null())
    return false;
  if (!data.ReadType(&out->type))
    return false;
  out->start_offset = data.start_offset();
  out->end_offset = data.end_offset();
  out->underline_color = data.underline_color();
  if (!data.ReadThickness(&out->thickness))
    return false;
  out->background_color = data.background_color();
  out->suggestion_highlight_color = data.suggestion_highlight_color();
  out->remove_on_finish_composing = data.remove_on_finish_composing();
  if (!data.ReadSuggestions(&out->suggestions))
    return false;
  return true;
}

// static
bool StructTraits<ws::mojom::CompositionTextDataView, ui::CompositionText>::
    Read(ws::mojom::CompositionTextDataView data, ui::CompositionText* out) {
  return !data.is_null() && data.ReadText(&out->text) &&
         data.ReadImeTextSpans(&out->ime_text_spans) &&
         data.ReadSelection(&out->selection);
}

// static
ws::mojom::FocusReason
EnumTraits<ws::mojom::FocusReason, ui::TextInputClient::FocusReason>::ToMojom(
    ui::TextInputClient::FocusReason input) {
  switch (input) {
    case ui::TextInputClient::FOCUS_REASON_NONE:
      return ws::mojom::FocusReason::kNone;
    case ui::TextInputClient::FOCUS_REASON_MOUSE:
      return ws::mojom::FocusReason::kMouse;
    case ui::TextInputClient::FOCUS_REASON_TOUCH:
      return ws::mojom::FocusReason::kTouch;
    case ui::TextInputClient::FOCUS_REASON_PEN:
      return ws::mojom::FocusReason::kPen;
    case ui::TextInputClient::FOCUS_REASON_OTHER:
      return ws::mojom::FocusReason::kOther;
  }

  NOTREACHED();
  return ws::mojom::FocusReason::kNone;
}

// static
bool EnumTraits<ws::mojom::FocusReason, ui::TextInputClient::FocusReason>::
    FromMojom(ws::mojom::FocusReason input,
              ui::TextInputClient::FocusReason* out) {
  switch (input) {
    case ws::mojom::FocusReason::kNone:
      *out = ui::TextInputClient::FOCUS_REASON_NONE;
      return true;
    case ws::mojom::FocusReason::kMouse:
      *out = ui::TextInputClient::FOCUS_REASON_MOUSE;
      return true;
    case ws::mojom::FocusReason::kTouch:
      *out = ui::TextInputClient::FOCUS_REASON_TOUCH;
      return true;
    case ws::mojom::FocusReason::kPen:
      *out = ui::TextInputClient::FOCUS_REASON_PEN;
      return true;
    case ws::mojom::FocusReason::kOther:
      *out = ui::TextInputClient::FOCUS_REASON_OTHER;
      return true;
  }

  NOTREACHED();
  return false;
}

// static
ws::mojom::ImeTextSpanType
EnumTraits<ws::mojom::ImeTextSpanType, ui::ImeTextSpan::Type>::ToMojom(
    ui::ImeTextSpan::Type ime_text_span_type) {
  switch (ime_text_span_type) {
    case ui::ImeTextSpan::Type::kComposition:
      return ws::mojom::ImeTextSpanType::kComposition;
    case ui::ImeTextSpan::Type::kSuggestion:
      return ws::mojom::ImeTextSpanType::kSuggestion;
    case ui::ImeTextSpan::Type::kMisspellingSuggestion:
      return ws::mojom::ImeTextSpanType::kMisspellingSuggestion;
  }

  NOTREACHED();
  return ws::mojom::ImeTextSpanType::kComposition;
}

// static
bool EnumTraits<ws::mojom::ImeTextSpanType, ui::ImeTextSpan::Type>::FromMojom(
    ws::mojom::ImeTextSpanType type,
    ui::ImeTextSpan::Type* out) {
  switch (type) {
    case ws::mojom::ImeTextSpanType::kComposition:
      *out = ui::ImeTextSpan::Type::kComposition;
      return true;
    case ws::mojom::ImeTextSpanType::kSuggestion:
      *out = ui::ImeTextSpan::Type::kSuggestion;
      return true;
    case ws::mojom::ImeTextSpanType::kMisspellingSuggestion:
      *out = ui::ImeTextSpan::Type::kMisspellingSuggestion;
      return true;
  }

  NOTREACHED();
  return false;
}

// static
ws::mojom::ImeTextSpanThickness EnumTraits<
    ws::mojom::ImeTextSpanThickness,
    ui::ImeTextSpan::Thickness>::ToMojom(ui::ImeTextSpan::Thickness thickness) {
  switch (thickness) {
    case ui::ImeTextSpan::Thickness::kNone:
      return ws::mojom::ImeTextSpanThickness::kNone;
    case ui::ImeTextSpan::Thickness::kThin:
      return ws::mojom::ImeTextSpanThickness::kThin;
    case ui::ImeTextSpan::Thickness::kThick:
      return ws::mojom::ImeTextSpanThickness::kThick;
  }

  NOTREACHED();
  return ws::mojom::ImeTextSpanThickness::kThin;
}

// static
bool EnumTraits<ws::mojom::ImeTextSpanThickness, ui::ImeTextSpan::Thickness>::
    FromMojom(ws::mojom::ImeTextSpanThickness input,
              ui::ImeTextSpan::Thickness* out) {
  switch (input) {
    case ws::mojom::ImeTextSpanThickness::kNone:
      *out = ui::ImeTextSpan::Thickness::kNone;
      return true;
    case ws::mojom::ImeTextSpanThickness::kThin:
      *out = ui::ImeTextSpan::Thickness::kThin;
      return true;
    case ws::mojom::ImeTextSpanThickness::kThick:
      *out = ui::ImeTextSpan::Thickness::kThick;
      return true;
  }

  NOTREACHED();
  return false;
}

// static
ws::mojom::TextInputMode
EnumTraits<ws::mojom::TextInputMode, ui::TextInputMode>::ToMojom(
    ui::TextInputMode text_input_mode) {
  switch (text_input_mode) {
    case ui::TEXT_INPUT_MODE_DEFAULT:
      return ws::mojom::TextInputMode::kDefault;
    case ui::TEXT_INPUT_MODE_NONE:
      return ws::mojom::TextInputMode::kNone;
    case ui::TEXT_INPUT_MODE_TEXT:
      return ws::mojom::TextInputMode::kText;
    case ui::TEXT_INPUT_MODE_TEL:
      return ws::mojom::TextInputMode::kTel;
    case ui::TEXT_INPUT_MODE_URL:
      return ws::mojom::TextInputMode::kUrl;
    case ui::TEXT_INPUT_MODE_EMAIL:
      return ws::mojom::TextInputMode::kEmail;
    case ui::TEXT_INPUT_MODE_NUMERIC:
      return ws::mojom::TextInputMode::kNumeric;
    case ui::TEXT_INPUT_MODE_DECIMAL:
      return ws::mojom::TextInputMode::kDecimal;
    case ui::TEXT_INPUT_MODE_SEARCH:
      return ws::mojom::TextInputMode::kSearch;
  }
  NOTREACHED();
  return ws::mojom::TextInputMode::kDefault;
}

// static
bool EnumTraits<ws::mojom::TextInputMode, ui::TextInputMode>::FromMojom(
    ws::mojom::TextInputMode input,
    ui::TextInputMode* out) {
  switch (input) {
    case ws::mojom::TextInputMode::kDefault:
      *out = ui::TEXT_INPUT_MODE_DEFAULT;
      return true;
    case ws::mojom::TextInputMode::kNone:
      *out = ui::TEXT_INPUT_MODE_NONE;
      return true;
    case ws::mojom::TextInputMode::kText:
      *out = ui::TEXT_INPUT_MODE_TEXT;
      return true;
    case ws::mojom::TextInputMode::kTel:
      *out = ui::TEXT_INPUT_MODE_TEL;
      return true;
    case ws::mojom::TextInputMode::kUrl:
      *out = ui::TEXT_INPUT_MODE_URL;
      return true;
    case ws::mojom::TextInputMode::kEmail:
      *out = ui::TEXT_INPUT_MODE_EMAIL;
      return true;
    case ws::mojom::TextInputMode::kNumeric:
      *out = ui::TEXT_INPUT_MODE_NUMERIC;
      return true;
    case ws::mojom::TextInputMode::kDecimal:
      *out = ui::TEXT_INPUT_MODE_DECIMAL;
      return true;
    case ws::mojom::TextInputMode::kSearch:
      *out = ui::TEXT_INPUT_MODE_SEARCH;
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
