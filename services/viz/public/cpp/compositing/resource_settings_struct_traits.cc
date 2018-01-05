// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/resource_settings_struct_traits.h"

namespace mojo {

// static
bool StructTraits<viz::mojom::ResourceSettingsDataView, viz::ResourceSettings>::
    Read(viz::mojom::ResourceSettingsDataView data,
         viz::ResourceSettings* out) {
  out->texture_id_allocation_chunk_size =
      data.texture_id_allocation_chunk_size();
  out->use_gpu_memory_buffer_resources = data.use_gpu_memory_buffer_resources();

  return true;
}

}  // namespace mojo
