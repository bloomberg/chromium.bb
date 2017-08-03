// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/resource_settings_struct_traits.h"

namespace mojo {

// static
bool StructTraits<viz::mojom::BufferToTextureTargetKeyDataView,
                  viz::BufferToTextureTargetKey>::
    Read(viz::mojom::BufferToTextureTargetKeyDataView data,
         viz::BufferToTextureTargetKey* out) {
  return data.ReadUsage(&out->first) && data.ReadFormat(&out->second);
}

// static
bool StructTraits<viz::mojom::ResourceSettingsDataView, viz::ResourceSettings>::
    Read(viz::mojom::ResourceSettingsDataView data,
         viz::ResourceSettings* out) {
  out->texture_id_allocation_chunk_size =
      data.texture_id_allocation_chunk_size();
  out->use_gpu_memory_buffer_resources = data.use_gpu_memory_buffer_resources();

  mojo::ArrayDataView<viz::mojom::BufferToTextureTargetPairDataView>
      buffer_to_texture_target_array;
  data.GetBufferToTextureTargetMapDataView(&buffer_to_texture_target_array);
  for (size_t i = 0; i < buffer_to_texture_target_array.size(); ++i) {
    viz::mojom::BufferToTextureTargetPairDataView buffer_to_texture_target_pair;
    buffer_to_texture_target_array.GetDataView(i,
                                               &buffer_to_texture_target_pair);
    viz::BufferToTextureTargetKey key;
    if (!buffer_to_texture_target_pair.ReadKey(&key))
      return false;

    auto& value = out->buffer_to_texture_target_map[key];
    value = buffer_to_texture_target_pair.value();
  }
  return true;
}

}  // namespace mojo
