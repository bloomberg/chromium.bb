// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/mojo/image_skia_struct_traits.h"

#include <string.h>

#include "base/logging.h"
#include "skia/public/interfaces/bitmap_skbitmap_struct_traits.h"

namespace mojo {

StructTraits<gfx::mojom::SharedBufferSkBitmapDataView,
             SkBitmap>::Context::Context() = default;
StructTraits<gfx::mojom::SharedBufferSkBitmapDataView,
             SkBitmap>::Context::~Context() = default;

// static
void* StructTraits<gfx::mojom::SharedBufferSkBitmapDataView,
                   SkBitmap>::SetUpContext(const SkBitmap& input) {
  // Shared buffer is not a good way for huge images. Consider alternatives.
  DCHECK_LT(input.width() * input.height(), 4000 * 4000);

  const std::vector<uint8_t> serialized_sk_bitmap(
      skia::mojom::Bitmap::Serialize(&input));

  // Use a context to serialize the bitmap to a shared buffer only once.
  Context* context = new Context;
  context->buffer_byte_size = serialized_sk_bitmap.size();
  context->shared_buffer_handle =
      mojo::SharedBufferHandle::Create(context->buffer_byte_size);
  mojo::ScopedSharedBufferMapping mapping =
      context->shared_buffer_handle->Map(context->buffer_byte_size);
  memcpy(mapping.get(), serialized_sk_bitmap.data(), context->buffer_byte_size);
  return context;
}

// static
void StructTraits<gfx::mojom::SharedBufferSkBitmapDataView,
                  SkBitmap>::TearDownContext(const SkBitmap& input,
                                             void* context) {
  delete static_cast<Context*>(context);
}

// static
mojo::ScopedSharedBufferHandle
StructTraits<gfx::mojom::SharedBufferSkBitmapDataView,
             SkBitmap>::shared_buffer_handle(const SkBitmap& input,
                                             void* context) {
  return (static_cast<Context*>(context))
      ->shared_buffer_handle->Clone(
          mojo::SharedBufferHandle::AccessMode::READ_ONLY);
}

// static
uint64_t StructTraits<gfx::mojom::SharedBufferSkBitmapDataView,
                      SkBitmap>::buffer_byte_size(const SkBitmap& input,
                                                  void* context) {
  return (static_cast<Context*>(context))->buffer_byte_size;
}

// static
bool StructTraits<gfx::mojom::SharedBufferSkBitmapDataView, SkBitmap>::Read(
    gfx::mojom::SharedBufferSkBitmapDataView data,
    SkBitmap* out) {
  mojo::ScopedSharedBufferHandle shared_buffer_handle =
      data.TakeSharedBufferHandle();
  if (!shared_buffer_handle.is_valid())
    return false;

  mojo::ScopedSharedBufferMapping mapping =
      shared_buffer_handle->Map(data.buffer_byte_size());
  if (!mapping)
    return false;

  const std::vector<uint8_t> serialized_sk_bitmap(
      static_cast<uint8_t*>(mapping.get()),
      static_cast<uint8_t*>(mapping.get()) + data.buffer_byte_size());

  return skia::mojom::Bitmap::Deserialize(serialized_sk_bitmap, out);
}

// static
float StructTraits<gfx::mojom::ImageSkiaRepDataView, gfx::ImageSkiaRep>::scale(
    const gfx::ImageSkiaRep& input) {
  const float scale = input.unscaled() ? 0.0f : input.scale();
  DCHECK_GE(scale, 0.0f);

  return scale;
}

// static
bool StructTraits<gfx::mojom::ImageSkiaRepDataView, gfx::ImageSkiaRep>::Read(
    gfx::mojom::ImageSkiaRepDataView data,
    gfx::ImageSkiaRep* out) {
  // An acceptable scale must be greater than or equal to 0.
  if (data.scale() < 0)
    return false;

  SkBitmap bitmap;
  if (!data.ReadBitmap(&bitmap))
    return false;

  *out = gfx::ImageSkiaRep(bitmap, data.scale());
  return true;
}

// static
std::vector<gfx::ImageSkiaRep>
StructTraits<gfx::mojom::ImageSkiaDataView, gfx::ImageSkia>::image_reps(
    const gfx::ImageSkia& input) {
  // Trigger the image to load everything.
  input.EnsureRepsForSupportedScales();
  return input.image_reps();
}

// static
bool StructTraits<gfx::mojom::ImageSkiaDataView, gfx::ImageSkia>::Read(
    gfx::mojom::ImageSkiaDataView data,
    gfx::ImageSkia* out) {
  DCHECK(out->isNull());

  std::vector<gfx::ImageSkiaRep> image_reps;
  if (!data.ReadImageReps(&image_reps))
    return false;

  for (const auto& image_rep : image_reps)
    out->AddRepresentation(image_rep);

  if (out->isNull())
    return false;

  out->SetReadOnly();
  return true;
}

}  // namespace mojo
