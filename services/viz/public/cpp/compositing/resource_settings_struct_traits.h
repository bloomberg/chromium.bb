// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_RESOURCE_SETTINGS_STRUCT_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_RESOURCE_SETTINGS_STRUCT_TRAITS_H_

#include <utility>

#include "components/viz/common/resources/resource_settings.h"
#include "services/viz/public/interfaces/compositing/resource_settings.mojom.h"
#include "ui/gfx/mojo/buffer_types.mojom.h"
#include "ui/gfx/mojo/buffer_types_struct_traits.h"

namespace mojo {

template <>
struct ArrayTraits<viz::BufferToTextureTargetMap> {
  using Element = viz::BufferToTextureTargetMap::value_type;
  using Iterator = viz::BufferToTextureTargetMap::iterator;
  using ConstIterator = viz::BufferToTextureTargetMap::const_iterator;

  static ConstIterator GetBegin(const viz::BufferToTextureTargetMap& input) {
    return input.begin();
  }
  static Iterator GetBegin(viz::BufferToTextureTargetMap& input) {  // NOLINT
    return input.begin();
  }

  static void AdvanceIterator(ConstIterator& iterator) {  // NOLINT
    iterator++;
  }
  static void AdvanceIterator(Iterator& iterator) {  // NOLINT
    iterator++;
  }

  static const Element& GetValue(ConstIterator& iterator) {  // NOLINT
    return *iterator;
  }
  static Element& GetValue(Iterator& iterator) { return *iterator; }  // NOLINT

  static size_t GetSize(const viz::BufferToTextureTargetMap& input) {
    return input.size();
  }
};

template <>
struct StructTraits<viz::mojom::BufferToTextureTargetKeyDataView,
                    std::pair<gfx::BufferUsage, gfx::BufferFormat>> {
  static gfx::BufferUsage usage(const viz::BufferToTextureTargetKey& input) {
    return input.first;
  }

  static gfx::BufferFormat format(const viz::BufferToTextureTargetKey& input) {
    return input.second;
  }

  static bool Read(viz::mojom::BufferToTextureTargetKeyDataView data,
                   viz::BufferToTextureTargetKey* out);
};

template <>
struct StructTraits<viz::mojom::BufferToTextureTargetPairDataView,
                    viz::BufferToTextureTargetMap::value_type> {
  static const std::pair<gfx::BufferUsage, gfx::BufferFormat>& key(
      const viz::BufferToTextureTargetMap::value_type& input) {
    return input.first;
  }

  static uint32_t value(
      const viz::BufferToTextureTargetMap::value_type& input) {
    return input.second;
  }
};

template <>
struct StructTraits<viz::mojom::ResourceSettingsDataView,
                    viz::ResourceSettings> {
  static size_t texture_id_allocation_chunk_size(
      const viz::ResourceSettings& input) {
    return input.texture_id_allocation_chunk_size;
  }

  static bool use_gpu_memory_buffer_resources(
      const viz::ResourceSettings& input) {
    return input.use_gpu_memory_buffer_resources;
  }

  static const viz::BufferToTextureTargetMap& buffer_to_texture_target_map(
      const viz::ResourceSettings& input) {
    return input.buffer_to_texture_target_map;
  }

  static bool Read(viz::mojom::ResourceSettingsDataView data,
                   viz::ResourceSettings* out);
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_RESOURCE_SETTINGS_STRUCT_TRAITS_H_
