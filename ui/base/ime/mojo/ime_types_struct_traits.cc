// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/mojo/ime_types_struct_traits.h"

namespace mojo {

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

// static
ui::mojom::TextInputMode
EnumTraits<ui::mojom::TextInputMode, ui::TextInputMode>::ToMojom(
    ui::TextInputMode text_input_mode) {
  switch (text_input_mode) {
    case ui::TEXT_INPUT_MODE_DEFAULT:
      return ui::mojom::TextInputMode::kDefault;
    case ui::TEXT_INPUT_MODE_NONE:
      return ui::mojom::TextInputMode::kNone;
    case ui::TEXT_INPUT_MODE_TEXT:
      return ui::mojom::TextInputMode::kText;
    case ui::TEXT_INPUT_MODE_TEL:
      return ui::mojom::TextInputMode::kTel;
    case ui::TEXT_INPUT_MODE_URL:
      return ui::mojom::TextInputMode::kUrl;
    case ui::TEXT_INPUT_MODE_EMAIL:
      return ui::mojom::TextInputMode::kEmail;
    case ui::TEXT_INPUT_MODE_NUMERIC:
      return ui::mojom::TextInputMode::kNumeric;
    case ui::TEXT_INPUT_MODE_DECIMAL:
      return ui::mojom::TextInputMode::kDecimal;
    case ui::TEXT_INPUT_MODE_SEARCH:
      return ui::mojom::TextInputMode::kSearch;
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
    case ui::mojom::TextInputMode::kNone:
      *out = ui::TEXT_INPUT_MODE_NONE;
      return true;
    case ui::mojom::TextInputMode::kText:
      *out = ui::TEXT_INPUT_MODE_TEXT;
      return true;
    case ui::mojom::TextInputMode::kTel:
      *out = ui::TEXT_INPUT_MODE_TEL;
      return true;
    case ui::mojom::TextInputMode::kUrl:
      *out = ui::TEXT_INPUT_MODE_URL;
      return true;
    case ui::mojom::TextInputMode::kEmail:
      *out = ui::TEXT_INPUT_MODE_EMAIL;
      return true;
    case ui::mojom::TextInputMode::kNumeric:
      *out = ui::TEXT_INPUT_MODE_NUMERIC;
      return true;
    case ui::mojom::TextInputMode::kDecimal:
      *out = ui::TEXT_INPUT_MODE_DECIMAL;
      return true;
    case ui::mojom::TextInputMode::kSearch:
      *out = ui::TEXT_INPUT_MODE_SEARCH;
      return true;
  }
  return false;
}

}  // namespace mojo
