// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/resource_settings_struct_traits.h"

namespace mojo {

// static
bool StructTraits<viz::mojom::BufferUsageAndFormatDataView,
                  viz::BufferUsageAndFormat>::
    Read(viz::mojom::BufferUsageAndFormatDataView data,
         viz::BufferUsageAndFormat* out) {
  return data.ReadUsage(&out->first) && data.ReadFormat(&out->second);
}

// static
bool StructTraits<viz::mojom::ResourceSettingsDataView, viz::ResourceSettings>::
    Read(viz::mojom::ResourceSettingsDataView data,
         viz::ResourceSettings* out) {
  out->texture_id_allocation_chunk_size =
      data.texture_id_allocation_chunk_size();
  out->use_gpu_memory_buffer_resources = data.use_gpu_memory_buffer_resources();

  mojo::ArrayDataView<viz::mojom::BufferUsageAndFormatDataView>
      usage_and_format_list;
  data.GetTextureTargetExceptionListDataView(&usage_and_format_list);
  for (size_t i = 0; i < usage_and_format_list.size(); ++i) {
    viz::mojom::BufferUsageAndFormatDataView usage_format_view;
    usage_and_format_list.GetDataView(i, &usage_format_view);
    viz::BufferUsageAndFormat usage_format;
    if (!usage_format_view.ReadUsage(&usage_format.first) ||
        !usage_format_view.ReadFormat(&usage_format.second))
      return false;

    out->texture_target_exception_list.push_back(usage_format);
  }

  return true;
}

}  // namespace mojo
