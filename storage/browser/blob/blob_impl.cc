// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_impl.h"

#include "storage/browser/blob/blob_data_handle.h"

namespace storage {

// static
base::WeakPtr<BlobImpl> BlobImpl::Create(std::unique_ptr<BlobDataHandle> handle,
                                         mojom::BlobRequest request) {
  return (new BlobImpl(std::move(handle), std::move(request)))
      ->weak_ptr_factory_.GetWeakPtr();
}

void BlobImpl::Clone(mojom::BlobRequest request) {
  bindings_.AddBinding(this, std::move(request));
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
                   mojom::BlobRequest request)
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
