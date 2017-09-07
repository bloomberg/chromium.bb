// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_TEXTURE_MAILBOX_STRUCT_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_TEXTURE_MAILBOX_STRUCT_TRAITS_H_

#include "build/build_config.h"
#include "components/viz/common/quads/texture_mailbox.h"
#include "gpu/ipc/common/mailbox_holder_struct_traits.h"
#include "services/viz/public/interfaces/compositing/texture_mailbox.mojom-shared.h"
#include "ui/gfx/geometry/mojo/geometry_struct_traits.h"
#include "ui/gfx/ipc/color/gfx_param_traits.h"

namespace mojo {

template <>
struct StructTraits<viz::mojom::TextureMailboxDataView, viz::TextureMailbox> {
  static const gpu::MailboxHolder& mailbox_holder(
      const viz::TextureMailbox& input) {
    return input.mailbox_holder_;
  }

  static const gfx::Size& size_in_pixels(const viz::TextureMailbox& input) {
    return input.size_in_pixels_;
  }

  static bool is_overlay_candidate(const viz::TextureMailbox& input) {
    return input.is_overlay_candidate_;
  }

  static bool is_backed_by_surface_texture(const viz::TextureMailbox& input) {
#if defined(OS_ANDROID)
    return input.is_backed_by_surface_texture_;
#else
    return false;
#endif
  }

  static bool wants_promotion_hint(const viz::TextureMailbox& input) {
#if defined(OS_ANDROID)
    return input.wants_promotion_hint_;
#else
    return false;
#endif
  }

  static bool secure_output_only(const viz::TextureMailbox& input) {
    return input.secure_output_only_;
  }

  static bool nearest_neighbor(const viz::TextureMailbox& input) {
    return input.nearest_neighbor_;
  }

  static const gfx::ColorSpace& color_space(const viz::TextureMailbox& input) {
    return input.color_space_;
  }

  static bool Read(viz::mojom::TextureMailboxDataView data,
                   viz::TextureMailbox* out) {
#if defined(OS_ANDROID)
    out->is_backed_by_surface_texture_ = data.is_backed_by_surface_texture();
    out->wants_promotion_hint_ = data.wants_promotion_hint();
#endif
    out->is_overlay_candidate_ = data.is_overlay_candidate();
    out->secure_output_only_ = data.secure_output_only();
    out->nearest_neighbor_ = data.nearest_neighbor();

    return data.ReadMailboxHolder(&out->mailbox_holder_) &&
           data.ReadSizeInPixels(&out->size_in_pixels_) &&
           data.ReadColorSpace(&out->color_space_);
  }
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_TEXTURE_MAILBOX_STRUCT_TRAITS_H_
