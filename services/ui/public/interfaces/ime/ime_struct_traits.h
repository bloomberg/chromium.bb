// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_PUBLIC_INTERFACES_IME_IME_STRUCT_TRAITS_H_
#define SERVICES_UI_PUBLIC_INTERFACES_IME_IME_STRUCT_TRAITS_H_

#include "base/strings/utf_string_conversions.h"
#include "services/ui/public/interfaces/ime/ime.mojom-shared.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/composition_underline.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"

namespace mojo {

template <>
struct StructTraits<ui::mojom::CompositionUnderlineDataView,
                    ui::CompositionUnderline> {
  static uint32_t start_offset(const ui::CompositionUnderline& c) {
    return c.start_offset;
  }
  static uint32_t end_offset(const ui::CompositionUnderline& c) {
    return c.end_offset;
  }
  static uint32_t color(const ui::CompositionUnderline& c) { return c.color; }
  static uint32_t thick(const ui::CompositionUnderline& c) { return c.thick; }
  static uint32_t background_color(const ui::CompositionUnderline& c) {
    return c.background_color;
  }
  static bool Read(ui::mojom::CompositionUnderlineDataView data,
                   ui::CompositionUnderline* out);
};

template <>
struct StructTraits<ui::mojom::CompositionTextDataView, ui::CompositionText> {
  static std::string text(const ui::CompositionText& c) {
    return base::UTF16ToUTF8(c.text);
  }
  static ui::CompositionUnderlines underlines(const ui::CompositionText& c) {
    return c.underlines;
  }
  static gfx::Range selection(const ui::CompositionText& c) {
    return c.selection;
  }
  static bool Read(ui::mojom::CompositionTextDataView data,
                   ui::CompositionText* out);
};

template <>
struct EnumTraits<ui::mojom::TextInputMode, ui::TextInputMode> {
  static ui::mojom::TextInputMode ToMojom(ui::TextInputMode text_input_mode);
  static bool FromMojom(ui::mojom::TextInputMode input, ui::TextInputMode* out);
};

template <>
struct EnumTraits<ui::mojom::TextInputType, ui::TextInputType> {
  static ui::mojom::TextInputType ToMojom(ui::TextInputType text_input_type);
  static bool FromMojom(ui::mojom::TextInputType input, ui::TextInputType* out);
};

}  // namespace mojo

#endif  // SERVICES_UI_PUBLIC_INTERFACES_IME_IME_STRUCT_TRAITS_H_
