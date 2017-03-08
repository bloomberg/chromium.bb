// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mojo/display_constants_struct_traits.h"

namespace mojo {

display::mojom::DisplayConnectionType EnumTraits<
    display::mojom::DisplayConnectionType,
    display::DisplayConnectionType>::ToMojom(display::DisplayConnectionType
                                                 type) {
  switch (type) {
    case display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_NONE:
      return display::mojom::DisplayConnectionType::
          DISPLAY_CONNECTION_TYPE_NONE;

    case display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_UNKNOWN:
      return display::mojom::DisplayConnectionType::
          DISPLAY_CONNECTION_TYPE_UNKNOWN;

    case display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_INTERNAL:
      return display::mojom::DisplayConnectionType::
          DISPLAY_CONNECTION_TYPE_INTERNAL;

    case display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_VGA:
      return display::mojom::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_VGA;

    case display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_HDMI:
      return display::mojom::DisplayConnectionType::
          DISPLAY_CONNECTION_TYPE_HDMI;

    case display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_DVI:
      return display::mojom::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_DVI;

    case display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_DISPLAYPORT:
      return display::mojom::DisplayConnectionType::
          DISPLAY_CONNECTION_TYPE_DISPLAYPORT;

    case display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_NETWORK:
      return display::mojom::DisplayConnectionType::
          DISPLAY_CONNECTION_TYPE_NETWORK;

    case display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_VIRTUAL:
      return display::mojom::DisplayConnectionType::
          DISPLAY_CONNECTION_TYPE_VIRTUAL;
  }
  NOTREACHED();
  return display::mojom::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_NONE;
}

bool EnumTraits<display::mojom::DisplayConnectionType,
                display::DisplayConnectionType>::
    FromMojom(display::mojom::DisplayConnectionType type,
              display::DisplayConnectionType* out) {
  switch (type) {
    case display::mojom::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_NONE:
      *out = display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_NONE;
      return true;

    case display::mojom::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_UNKNOWN:
      *out = display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_UNKNOWN;
      return true;

    case display::mojom::DisplayConnectionType::
        DISPLAY_CONNECTION_TYPE_INTERNAL:
      *out = display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_INTERNAL;
      return true;

    case display::mojom::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_VGA:
      *out = display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_VGA;
      return true;

    case display::mojom::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_HDMI:
      *out = display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_HDMI;
      return true;

    case display::mojom::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_DVI:
      *out = display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_DVI;
      return true;

    case display::mojom::DisplayConnectionType::
        DISPLAY_CONNECTION_TYPE_DISPLAYPORT:
      *out =
          display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_DISPLAYPORT;
      return true;

    case display::mojom::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_NETWORK:
      *out = display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_NETWORK;
      return true;

    case display::mojom::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_VIRTUAL:
      *out = display::DisplayConnectionType::DISPLAY_CONNECTION_TYPE_VIRTUAL;
      return true;
  }
  return false;
}

}  // namespace mojo
