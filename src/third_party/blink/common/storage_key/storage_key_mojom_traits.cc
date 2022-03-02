// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/storage_key/storage_key_mojom_traits.h"

#include "base/stl_util.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/cpp/schemeful_site_mojom_traits.h"
#include "url/mojom/origin_mojom_traits.h"
#include "url/origin.h"

namespace mojo {

// static
bool StructTraits<blink::mojom::StorageKeyDataView, blink::StorageKey>::Read(
    blink::mojom::StorageKeyDataView data,
    blink::StorageKey* out) {
  url::Origin origin;
  if (!data.ReadOrigin(&origin))
    return false;

  net::SchemefulSite top_level_site;
  if (!data.ReadTopLevelSite(&top_level_site))
    return false;

  absl::optional<base::UnguessableToken> nonce;
  if (!data.ReadNonce(&nonce))
    return false;

  *out = blink::StorageKey::CreateWithOptionalNonce(
      origin, top_level_site, base::OptionalOrNullptr(nonce));
  return true;
}

}  // namespace mojo
