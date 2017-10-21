// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BlobBytesProvider_h
#define BlobBytesProvider_h

#include "platform/blob/BlobData.h"
#include "third_party/WebKit/common/blob/blob_registry.mojom-blink.h"

namespace blink {

// Implementation of the BytesProvider mojo interface, used to transport bytes
// making up a blob to the browser process, at the request of the blob service.
//
// Typical usage of this class creates and calls AppendData on one thread, and
// then transfers ownership of the class to a different thread where it will be
// bound to a mojo pipe, such that the various Request* methods are called on a
// thread that is allowed to do File IO.
class PLATFORM_EXPORT BlobBytesProvider : public mojom::blink::BytesProvider {
 public:
  explicit BlobBytesProvider(scoped_refptr<RawData>);
  ~BlobBytesProvider() override;

  void AppendData(scoped_refptr<RawData>);

  // BytesProvider implementation:
  void RequestAsReply(RequestAsReplyCallback) override;
  void RequestAsStream(mojo::ScopedDataPipeProducerHandle) override;
  void RequestAsFile(uint64_t source_offset,
                     uint64_t source_size,
                     base::File,
                     uint64_t file_offset,
                     RequestAsFileCallback) override;

 private:
  Vector<scoped_refptr<RawData>> data_;
};

}  // namespace blink

#endif  // BlobBytesProvider_h
