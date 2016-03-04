// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_MOJO_ORIGIN_STRUCT_TRAITS_H_
#define URL_MOJO_ORIGIN_STRUCT_TRAITS_H_

#include "base/strings/string_piece.h"
#include "url/mojo/origin.mojom.h"
#include "url/origin.h"

namespace mojo {

template <>
struct StructTraits<url::mojom::Origin, url::Origin> {
  static base::StringPiece scheme(const url::Origin& r) {
    return base::StringPiece(r.scheme());
  }
  static base::StringPiece host(const url::Origin& r) {
    return base::StringPiece(r.host());
  }
  static uint16_t port(const url::Origin& r) {
    return r.port();
  }
  static bool unique(const url::Origin& r) {
    return r.unique();
  }
  static bool Read(url::mojom::Origin::Reader r, url::Origin* out) {
    *out = r.unique() ? url::Origin()
                      : url::Origin::UnsafelyCreateOriginWithoutNormalization(
                            r.scheme(), r.host(), r.port());

    // If a unique origin was created, but the unique flag wasn't set, then
    // the values provided to 'UnsafelyCreateOriginWithoutNormalization' were
    // invalid.
    if (!r.unique() && out->unique())
      return false;

    return true;
  }
};

}

#endif  // URL_MOJO_ORIGIN_STRUCT_TRAITS_H_
