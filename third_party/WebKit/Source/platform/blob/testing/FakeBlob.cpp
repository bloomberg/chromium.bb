// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/blob/testing/FakeBlob.h"

#include "mojo/public/cpp/bindings/strong_binding.h"

namespace blink {

FakeBlob::FakeBlob(const String& uuid) : uuid_(uuid) {}

void FakeBlob::Clone(mojom::blink::BlobRequest request) {
  mojo::MakeStrongBinding(std::make_unique<FakeBlob>(uuid_),
                          std::move(request));
}

void FakeBlob::ReadRange(uint64_t offset,
                         uint64_t length,
                         mojo::ScopedDataPipeProducerHandle,
                         mojom::blink::BlobReaderClientPtr) {
  NOTREACHED();
}

void FakeBlob::ReadAll(mojo::ScopedDataPipeProducerHandle,
                       mojom::blink::BlobReaderClientPtr) {
  NOTREACHED();
}

void FakeBlob::GetInternalUUID(GetInternalUUIDCallback callback) {
  std::move(callback).Run(uuid_);
}

}  // namespace blink
