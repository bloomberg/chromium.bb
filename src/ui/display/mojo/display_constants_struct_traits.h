// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MOJO_DISPLAY_CONSTANTS_STRUCT_TRAITS_H_
#define UI_DISPLAY_MOJO_DISPLAY_CONSTANTS_STRUCT_TRAITS_H_

#include "ui/display/mojo/display_constants.mojom.h"
#include "ui/display/types/display_constants.h"

namespace mojo {

template <>
struct EnumTraits<display::mojom::DisplayConnectionType,
                  display::DisplayConnectionType> {
  static display::mojom::DisplayConnectionType ToMojom(
      display::DisplayConnectionType type);
  static bool FromMojom(display::mojom::DisplayConnectionType type,
                        display::DisplayConnectionType* out);
};

template <>
struct EnumTraits<display::mojom::HDCPState, display::HDCPState> {
  static display::mojom::HDCPState ToMojom(display::HDCPState type);
  static bool FromMojom(display::mojom::HDCPState type,
                        display::HDCPState* out);
};

}  // namespace mojo

#endif  // UI_DISPLAY_MOJO_DISPLAY_CONSTANTS_STRUCT_TRAITS_H_
