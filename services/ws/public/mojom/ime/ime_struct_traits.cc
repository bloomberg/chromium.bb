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

}  // namespace mojo
