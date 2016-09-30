// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJO_ICC_PROFILE_STRUCT_TRAITS_H_
#define UI_GFX_MOJO_ICC_PROFILE_STRUCT_TRAITS_H_

#include <algorithm>
#include <memory>

#include "base/strings/string_piece.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "ui/gfx/icc_profile.h"
#include "ui/gfx/mojo/icc_profile.mojom.h"

namespace mojo {

template <>
struct StructTraits<gfx::mojom::ICCProfileDataView, gfx::ICCProfile> {
  static bool valid(const gfx::ICCProfile& profile) { return profile.valid_; }
  static base::StringPiece data(const gfx::ICCProfile& profile) {
    return base::StringPiece(profile.data_.data(), profile.data_.size());
  }
  static uint64_t id(const gfx::ICCProfile& profile) { return profile.id_; }

  static bool Read(gfx::mojom::ICCProfileDataView data,
                   gfx::ICCProfile* profile);
};

}  // namespace mojo

#endif  // UI_GFX_MOJO_ICC_PROFILE_STRUCT_TRAITS_H_
