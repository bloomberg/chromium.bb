// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_PUBLIC_MOJOM_IME_IME_STRUCT_TRAITS_H_
#define SERVICES_WS_PUBLIC_MOJOM_IME_IME_STRUCT_TRAITS_H_

#include "services/ws/public/mojom/ime/ime.mojom-shared.h"
#include "ui/base/ime/candidate_window.h"

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

}  // namespace mojo

#endif  // SERVICES_WS_PUBLIC_MOJOM_IME_IME_STRUCT_TRAITS_H_
