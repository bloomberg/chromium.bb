// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_piece.h"
#include "url/gurl.h"
#include "url/mojo/url.mojom.h"

namespace mojo {

// copied from content/public/common/content_constants.cc: make that file use
// this definition.
const size_t kMaxUrlChars = 2 * 1024 * 1024;

template <>
struct StructTraits<url::mojom::Url, GURL> {
  static base::StringPiece url(const GURL& r) {
    if (r.possibly_invalid_spec().length() > kMaxUrlChars || !r.is_valid()) {
      return base::StringPiece();
    }

    return base::StringPiece(r.possibly_invalid_spec().c_str(),
                             r.possibly_invalid_spec().length());
  }
  static bool Read(url::mojom::Url::Reader r, GURL* out) {
    if (r.url().length() > kMaxUrlChars) {
      *out = GURL();
      return false;
    }
    *out = GURL(r.url());
    if (!r.url().empty() && !out->is_valid()) {
      *out = GURL();
      return false;
    }
    return true;
  }
};

}
