// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_impl.h"

#include <utility>
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/mojo_blob_reader.h"

namespace storage {
namespace {

class ReaderDelegate : public MojoBlobReader::Delegate {
 public:
  ReaderDelegate(mojo::ScopedDataPipeProducerHandle handle,
                 blink::mojom::BlobReaderClientPtr client)
      : handle_(std::move(handle)), client_(std::move(client)) {}

  mojo::ScopedDataPipeProducerHandle PassDataPipe() override {
    return std::move(handle_);
  }

  MojoBlobReader::Delegate::RequestSideData DidCalculateSize(
      uint64_t total_size,
      uint64_t content_size) override {
    if (client_)
      client_->OnCalculatedSize(total_size, content_size);
    return MojoBlobReader::Delegate::DONT_REQUEST_SIDE_DATA;
  }

  void OnComplete(net::Error result, uint64_t total_written_bytes) override {
    if (client_)
      client_->OnComplete(result, total_written_bytes);
  }

 private:
  mojo::ScopedDataPipeProducerHandle handle_;
  blink::mojom::BlobReaderClientPtr client_;
};

}  // namespace

// static
base::WeakPtr<BlobImpl> BlobImpl::Create(std::unique_ptr<BlobDataHandle> handle,
                                         blink::mojom::BlobRequest request) {
  return (new BlobImpl(std::move(handle), std::move(request)))
      ->weak_ptr_factory_.GetWeakPtr();
}

void BlobImpl::Clone(blink::mojom::BlobRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void BlobImpl::ReadRange(uint64_t offset,
                         uint64_t length,
                         mojo::ScopedDataPipeProducerHandle handle,
                         blink::mojom::BlobReaderClientPtr client) {
  MojoBlobReader::Create(
      nullptr, handle_.get(),
      net::HttpByteRange::Bounded(offset, offset + length - 1),
      base::MakeUnique<ReaderDelegate>(std::move(handle), std::move(client)));
}

void BlobImpl::ReadAll(mojo::ScopedDataPipeProducerHandle handle,
                       blink::mojom::BlobReaderClientPtr client) {
  MojoBlobReader::Create(
      nullptr, handle_.get(), net::HttpByteRange(),
      base::MakeUnique<ReaderDelegate>(std::move(handle), std::move(client)));
}

void BlobImpl::GetInternalUUID(GetInternalUUIDCallback callback) {
  std::move(callback).Run(handle_->uuid());
}

void BlobImpl::FlushForTesting() {
  bindings_.FlushForTesting();
  if (bindings_.empty())
    delete this;
}

BlobImpl::BlobImpl(std::unique_ptr<BlobDataHandle> handle,
                   blink::mojom::BlobRequest request)
    : handle_(std::move(handle)), weak_ptr_factory_(this) {
  DCHECK(handle_);
  bindings_.AddBinding(this, std::move(request));
  bindings_.set_connection_error_handler(
      base::Bind(&BlobImpl::OnConnectionError, base::Unretained(this)));
}

BlobImpl::~BlobImpl() {}

void BlobImpl::OnConnectionError() {
  if (!bindings_.empty())
    return;
  delete this;
}

}  // namespace storage
