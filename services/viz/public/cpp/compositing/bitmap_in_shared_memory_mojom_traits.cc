// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/bitmap_in_shared_memory_mojom_traits.h"

namespace {

void DeleteSharedMemoryMapping(void* not_used, void* context) {
  delete static_cast<base::WritableSharedMemoryMapping*>(context);
}

}  // namespace

namespace mojo {

// static
const SkImageInfo&
StructTraits<viz::mojom::BitmapInSharedMemoryDataView, SkBitmap>::image_info(
    const SkBitmap& sk_bitmap) {
  return sk_bitmap.info();
}

// static
uint64_t StructTraits<viz::mojom::BitmapInSharedMemoryDataView,
                      SkBitmap>::row_bytes(const SkBitmap& sk_bitmap) {
  return sk_bitmap.rowBytes();
}

// static
base::Optional<base::WritableSharedMemoryRegion>
StructTraits<viz::mojom::BitmapInSharedMemoryDataView, SkBitmap>::pixels(
    const SkBitmap& sk_bitmap) {
  if (!sk_bitmap.readyToDraw())
    return base::nullopt;

  size_t byte_size = sk_bitmap.computeByteSize();
  base::WritableSharedMemoryRegion region =
      base::WritableSharedMemoryRegion::Create(byte_size);
  {
    base::WritableSharedMemoryMapping mapping = region.Map();
    if (!mapping.IsValid())
      return base::nullopt;
    memcpy(mapping.memory(), sk_bitmap.getPixels(), byte_size);
  }
  return region;
}

// static
bool StructTraits<viz::mojom::BitmapInSharedMemoryDataView, SkBitmap>::Read(
    viz::mojom::BitmapInSharedMemoryDataView data,
    SkBitmap* sk_bitmap) {
  SkImageInfo image_info;
  if (!data.ReadImageInfo(&image_info))
    return false;
  if (!image_info.validRowBytes(data.row_bytes()))
    return false;

  base::Optional<base::WritableSharedMemoryRegion> region_opt;
  if (!data.ReadPixels(&region_opt))
    return false;

  *sk_bitmap = SkBitmap();
  if (!region_opt)
    return sk_bitmap->setInfo(image_info, data.row_bytes());

  auto mapping_ptr =
      std::make_unique<base::WritableSharedMemoryMapping>(region_opt->Map());
  if (!mapping_ptr->IsValid())
    return false;

  if (!sk_bitmap->installPixels(image_info, mapping_ptr->memory(),
                                data.row_bytes(), &DeleteSharedMemoryMapping,
                                mapping_ptr.get())) {
    return false;
  }
  mapping_ptr.release();
  return true;
}

}  // namespace mojo
