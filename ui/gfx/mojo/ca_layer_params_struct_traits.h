// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJO_CA_LAYER_PARAMS_STRUCT_TRAITS_H_
#define UI_GFX_MOJO_CA_LAYER_PARAMS_STRUCT_TRAITS_H_

#include "ui/gfx/ca_layer_params.h"
#include "ui/gfx/mojo/ca_layer_params.mojom.h"

namespace mojo {

template <>
struct StructTraits<gfx::mojom::CALayerParamsDataView, gfx::CALayerParams> {
  static uint32_t is_empty(const gfx::CALayerParams& ca_layer_params) {
    return ca_layer_params.is_empty;
  }

  static uint32_t ca_context_id(const gfx::CALayerParams& ca_layer_params) {
    return ca_layer_params.ca_context_id;
  }

  static mojo::ScopedHandle io_surface_mach_port(
      const gfx::CALayerParams& ca_layer_params);

  static gfx::Size pixel_size(const gfx::CALayerParams& ca_layer_params) {
    return ca_layer_params.pixel_size;
  }

  static float scale_factor(const gfx::CALayerParams& ca_layer_params) {
    return ca_layer_params.scale_factor;
  }

  static bool Read(gfx::mojom::CALayerParamsDataView data,
                   gfx::CALayerParams* out);
};

}  // namespace mojo

#endif  // UI_GFX_MOJO_CA_LAYER_PARAMS_STRUCT_TRAITS_H_
