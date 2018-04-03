// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FakeBlobURLStore_h
#define FakeBlobURLStore_h

#include "third_party/WebKit/public/mojom/blob/blob_url_store.mojom-blink.h"

#include "mojo/public/cpp/bindings/binding_set.h"
#include "platform/weborigin/KURLHash.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/Vector.h"

namespace blink {

// Mocked BlobURLStore implementation for testing.
class FakeBlobURLStore : public mojom::blink::BlobURLStore {
 public:
  void Register(mojom::blink::BlobPtr, const KURL&, RegisterCallback) override;
  void Revoke(const KURL&) override;
  void Resolve(const KURL&, ResolveCallback) override;
  void ResolveAsURLLoaderFactory(
      const KURL&,
      network::mojom::blink::URLLoaderFactoryRequest) override;
  void ResolveForNavigation(const KURL&,
                            mojom::blink::BlobURLTokenRequest) override;

  HashMap<KURL, mojom::blink::BlobPtr> registrations;
  Vector<KURL> revocations;
};

}  // namespace blink

#endif  // FakeBlobURLStore_h
