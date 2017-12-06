// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mojo/ca_layer_params_struct_traits.h"

#include "build/build_config.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/gfx/geometry/mojo/geometry_struct_traits.h"

namespace mojo {

mojo::ScopedHandle
StructTraits<gfx::mojom::CALayerParamsDataView, gfx::CALayerParams>::
    io_surface_mach_port(const gfx::CALayerParams& ca_layer_params) {
#if defined(OS_MACOSX) && !defined(OS_IOS)
  return mojo::WrapMachPort(ca_layer_params.io_surface_mach_port.get());
#else
  return mojo::ScopedHandle();
#endif
}

bool StructTraits<gfx::mojom::CALayerParamsDataView, gfx::CALayerParams>::Read(
    gfx::mojom::CALayerParamsDataView data,
    gfx::CALayerParams* out) {
  out->is_empty = data.is_empty();
  out->ca_context_id = data.ca_context_id();

#if defined(OS_MACOSX) && !defined(OS_IOS)
  mach_port_t io_surface_mach_port;
  MojoResult unwrap_result =
      mojo::UnwrapMachPort(data.TakeIoSurfaceMachPort(), &io_surface_mach_port);
  if (unwrap_result != MOJO_RESULT_OK)
    return false;
  out->io_surface_mach_port.reset(io_surface_mach_port);
#endif

  if (!data.ReadPixelSize(&out->pixel_size))
    return false;
  out->scale_factor = data.scale_factor();
  return true;
}

}  // namespace mojo
