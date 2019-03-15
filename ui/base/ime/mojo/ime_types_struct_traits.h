// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_MOJO_IME_TYPES_STRUCT_TRAITS_H_
#define UI_BASE_IME_MOJO_IME_TYPES_STRUCT_TRAITS_H_

#include <vector>

#include "ui/base/ime/mojo/ime_types.mojom.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"

namespace mojo {

template <>
struct EnumTraits<ui::mojom::TextInputType, ui::TextInputType> {
  static ui::mojom::TextInputType ToMojom(ui::TextInputType text_input_type);
  static bool FromMojom(ui::mojom::TextInputType input, ui::TextInputType* out);
};

template <>
struct EnumTraits<ui::mojom::TextInputMode, ui::TextInputMode> {
  static ui::mojom::TextInputMode ToMojom(ui::TextInputMode text_input_mode);
  static bool FromMojom(ui::mojom::TextInputMode input, ui::TextInputMode* out);
};

}  // namespace mojo

#endif  // UI_BASE_IME_MOJO_IME_TYPES_STRUCT_TRAITS_H_
