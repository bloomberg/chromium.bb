// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_PUBLIC_MOJOM_IME_IME_STRUCT_TRAITS_H_
#define SERVICES_WS_PUBLIC_MOJOM_IME_IME_STRUCT_TRAITS_H_

#include "services/ws/public/mojom/ime/ime.mojom-shared.h"
#include "ui/base/ime/candidate_window.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/ime_text_span.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"

namespace mojo {

template <>
struct StructTraits<ws::mojom::CandidateWindowPropertiesDataView,
                    ui::CandidateWindow::CandidateWindowProperty> {
  static int32_t page_size(
      const ui::CandidateWindow::CandidateWindowProperty& p) {
    return p.page_size;
  }
  static bool vertical(const ui::CandidateWindow::CandidateWindowProperty& p) {
    return p.is_vertical;
  }
  static std::string auxiliary_text(
      const ui::CandidateWindow::CandidateWindowProperty& p) {
    return p.auxiliary_text;
  }
  static bool auxiliary_text_visible(
      const ui::CandidateWindow::CandidateWindowProperty& p) {
    return p.is_auxiliary_text_visible;
  }
  static int32_t cursor_position(
      const ui::CandidateWindow::CandidateWindowProperty& p) {
    return p.cursor_position;
  }
  static bool cursor_visible(
      const ui::CandidateWindow::CandidateWindowProperty& p) {
    return p.is_cursor_visible;
  }
  static ws::mojom::CandidateWindowPosition window_position(
      const ui::CandidateWindow::CandidateWindowProperty& p) {
    return p.show_window_at_composition
               ? ws::mojom::CandidateWindowPosition::kComposition
               : ws::mojom::CandidateWindowPosition::kCursor;
  }
  static bool Read(ws::mojom::CandidateWindowPropertiesDataView data,
                   ui::CandidateWindow::CandidateWindowProperty* out);
};

template <>
struct StructTraits<ws::mojom::CandidateWindowEntryDataView,
                    ui::CandidateWindow::Entry> {
  static base::string16 value(const ui::CandidateWindow::Entry& e) {
    return e.value;
  }
  static base::string16 label(const ui::CandidateWindow::Entry& e) {
    return e.label;
  }
  static base::string16 annotation(const ui::CandidateWindow::Entry& e) {
    return e.annotation;
  }
  static base::string16 description_title(const ui::CandidateWindow::Entry& e) {
    return e.description_title;
  }
  static base::string16 description_body(const ui::CandidateWindow::Entry& e) {
    return e.description_body;
  }
  static bool Read(ws::mojom::CandidateWindowEntryDataView data,
                   ui::CandidateWindow::Entry* out);
};

template <>
struct StructTraits<ws::mojom::CompositionTextDataView, ui::CompositionText> {
  static base::string16 text(const ui::CompositionText& c) { return c.text; }
  static ui::ImeTextSpans ime_text_spans(const ui::CompositionText& c) {
    return c.ime_text_spans;
  }
  static gfx::Range selection(const ui::CompositionText& c) {
    return c.selection;
  }
  static bool Read(ws::mojom::CompositionTextDataView data,
                   ui::CompositionText* out);
};

template <>
struct EnumTraits<ws::mojom::FocusReason, ui::TextInputClient::FocusReason> {
  static ws::mojom::FocusReason ToMojom(ui::TextInputClient::FocusReason input);
  static bool FromMojom(ws::mojom::FocusReason input,
                        ui::TextInputClient::FocusReason* out);
};

template <>
struct StructTraits<ws::mojom::ImeTextSpanDataView, ui::ImeTextSpan> {
  static ui::ImeTextSpan::Type type(const ui::ImeTextSpan& c) { return c.type; }
  static uint32_t start_offset(const ui::ImeTextSpan& c) {
    return c.start_offset;
  }
  static uint32_t end_offset(const ui::ImeTextSpan& c) { return c.end_offset; }
  static uint32_t underline_color(const ui::ImeTextSpan& c) {
    return c.underline_color;
  }
  static ui::ImeTextSpan::Thickness thickness(const ui::ImeTextSpan& i) {
    return i.thickness;
  }
  static uint32_t background_color(const ui::ImeTextSpan& c) {
    return c.background_color;
  }
  static uint32_t suggestion_highlight_color(const ui::ImeTextSpan& c) {
    return c.suggestion_highlight_color;
  }
  static bool remove_on_finish_composing(const ui::ImeTextSpan& c) {
    return c.remove_on_finish_composing;
  }
  static std::vector<std::string> suggestions(const ui::ImeTextSpan& c) {
    return c.suggestions;
  }
  static bool Read(ws::mojom::ImeTextSpanDataView data, ui::ImeTextSpan* out);
};

template <>
struct EnumTraits<ws::mojom::ImeTextSpanType, ui::ImeTextSpan::Type> {
  static ws::mojom::ImeTextSpanType ToMojom(
      ui::ImeTextSpan::Type ime_text_span_type);
  static bool FromMojom(ws::mojom::ImeTextSpanType input,
                        ui::ImeTextSpan::Type* out);
};

template <>
struct EnumTraits<ws::mojom::ImeTextSpanThickness, ui::ImeTextSpan::Thickness> {
  static ws::mojom::ImeTextSpanThickness ToMojom(
      ui::ImeTextSpan::Thickness thickness);
  static bool FromMojom(ws::mojom::ImeTextSpanThickness input,
                        ui::ImeTextSpan::Thickness* out);
};

}  // namespace mojo

#endif  // SERVICES_WS_PUBLIC_MOJOM_IME_IME_STRUCT_TRAITS_H_
