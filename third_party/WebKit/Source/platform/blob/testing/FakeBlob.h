// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FakeBlob_h
#define FakeBlob_h

#include "third_party/WebKit/public/mojom/blob/blob.mojom-blink.h"

namespace blink {

// Mocked Blob implementation for testing. You can't read from a FakeBlob, but
// it does have a UUID.
class FakeBlob : public mojom::blink::Blob {
 public:
  explicit FakeBlob(const String& uuid);

  void Clone(mojom::blink::BlobRequest) override;
  void AsDataPipeGetter(network::mojom::blink::DataPipeGetterRequest) override;
  void ReadRange(uint64_t offset,
                 uint64_t length,
                 mojo::ScopedDataPipeProducerHandle,
                 mojom::blink::BlobReaderClientPtr) override;
  void ReadAll(mojo::ScopedDataPipeProducerHandle,
               mojom::blink::BlobReaderClientPtr) override;
  void ReadSideData(ReadSideDataCallback) override;
  void GetInternalUUID(GetInternalUUIDCallback) override;

 private:
  String uuid_;
};

}  // namespace blink

#endif  // FakeBlob_h
