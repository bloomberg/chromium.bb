// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_COPY_OUTPUT_RESULT_STRUCT_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_COPY_OUTPUT_RESULT_STRUCT_TRAITS_H_

#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "gpu/ipc/common/mailbox_struct_traits.h"
#include "gpu/ipc/common/sync_token_struct_traits.h"
#include "services/viz/public/interfaces/compositing/copy_output_result.mojom-shared.h"
#include "services/viz/public/interfaces/compositing/texture_releaser.mojom.h"
#include "skia/public/interfaces/bitmap_skbitmap_struct_traits.h"
#include "ui/gfx/geometry/mojo/geometry_struct_traits.h"
#include "ui/gfx/ipc/color/gfx_param_traits.h"

namespace mojo {

template <>
struct EnumTraits<viz::mojom::CopyOutputResultFormat,
                  viz::CopyOutputResult::Format> {
  static viz::mojom::CopyOutputResultFormat ToMojom(
      viz::CopyOutputResult::Format format) {
    switch (format) {
      case viz::CopyOutputResult::Format::RGBA_BITMAP:
        return viz::mojom::CopyOutputResultFormat::RGBA_BITMAP;
      case viz::CopyOutputResult::Format::RGBA_TEXTURE:
        return viz::mojom::CopyOutputResultFormat::RGBA_TEXTURE;
      case viz::CopyOutputResult::Format::I420_PLANES:
        break;  // Not intended for transport across service boundaries.
    }
    NOTREACHED();
    return viz::mojom::CopyOutputResultFormat::RGBA_BITMAP;
  }

  static bool FromMojom(viz::mojom::CopyOutputResultFormat input,
                        viz::CopyOutputResult::Format* out) {
    switch (input) {
      case viz::mojom::CopyOutputResultFormat::RGBA_BITMAP:
        *out = viz::CopyOutputResult::Format::RGBA_BITMAP;
        return true;
      case viz::mojom::CopyOutputResultFormat::RGBA_TEXTURE:
        *out = viz::CopyOutputResult::Format::RGBA_TEXTURE;
        return true;
    }
    return false;
  }
};

template <>
struct StructTraits<viz::mojom::CopyOutputResultDataView,
                    std::unique_ptr<viz::CopyOutputResult>> {
  static viz::CopyOutputResult::Format format(
      const std::unique_ptr<viz::CopyOutputResult>& result) {
    return result->format();
  }

  static const gfx::Rect& rect(
      const std::unique_ptr<viz::CopyOutputResult>& result) {
    return result->rect();
  }

  static const SkBitmap& bitmap(
      const std::unique_ptr<viz::CopyOutputResult>& result) {
    // This will return a non-drawable bitmap if the result was not
    // RGBA_BITMAP or if the result is empty.
    return result->AsSkBitmap();
  }

  static base::Optional<gpu::Mailbox> mailbox(
      const std::unique_ptr<viz::CopyOutputResult>& result) {
    if (result->format() != viz::CopyOutputResult::Format::RGBA_TEXTURE)
      return base::nullopt;
    return result->GetTextureResult()->mailbox;
  }

  static base::Optional<gpu::SyncToken> sync_token(
      const std::unique_ptr<viz::CopyOutputResult>& result) {
    if (result->format() != viz::CopyOutputResult::Format::RGBA_TEXTURE)
      return base::nullopt;
    return result->GetTextureResult()->sync_token;
  }

  static base::Optional<gfx::ColorSpace> color_space(
      const std::unique_ptr<viz::CopyOutputResult>& result) {
    if (result->format() != viz::CopyOutputResult::Format::RGBA_TEXTURE)
      return base::nullopt;
    return result->GetTextureResult()->color_space;
  }

  static viz::mojom::TextureReleaserPtr releaser(
      const std::unique_ptr<viz::CopyOutputResult>& result);

  static bool Read(viz::mojom::CopyOutputResultDataView data,
                   std::unique_ptr<viz::CopyOutputResult>* out_p);

 private:
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_COPY_OUTPUT_RESULT_STRUCT_TRAITS_H_
