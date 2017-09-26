// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_COPY_OUTPUT_RESULT_STRUCT_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_COPY_OUTPUT_RESULT_STRUCT_TRAITS_H_

#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "services/viz/public/cpp/compositing/texture_mailbox_struct_traits.h"
#include "services/viz/public/interfaces/compositing/copy_output_result.mojom-shared.h"
#include "services/viz/public/interfaces/compositing/texture_mailbox_releaser.mojom.h"
#include "skia/public/interfaces/bitmap_skbitmap_struct_traits.h"
#include "ui/gfx/geometry/mojo/geometry_struct_traits.h"

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
    return result->AsSkBitmap();
  }

  static base::Optional<viz::TextureMailbox> texture_mailbox(
      const std::unique_ptr<viz::CopyOutputResult>& result) {
    if (HasTextureResult(*result))
      return *result->GetTextureMailbox();
    else
      return base::nullopt;
  }

  static viz::mojom::TextureMailboxReleaserPtr releaser(
      const std::unique_ptr<viz::CopyOutputResult>& result);

  static bool Read(viz::mojom::CopyOutputResultDataView data,
                   std::unique_ptr<viz::CopyOutputResult>* out_p);

 private:
  static bool HasTextureResult(const viz::CopyOutputResult& result);
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_COPY_OUTPUT_RESULT_STRUCT_TRAITS_H_
