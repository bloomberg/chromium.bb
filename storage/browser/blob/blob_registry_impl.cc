// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_registry_impl.h"

#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"

namespace storage {

BlobRegistryImpl::BlobRegistryImpl(BlobStorageContext* context)
    : context_(context), weak_ptr_factory_(this) {}

BlobRegistryImpl::~BlobRegistryImpl() {}

void BlobRegistryImpl::Bind(mojom::BlobRegistryRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void BlobRegistryImpl::Register(mojom::BlobRequest blob,
                                const std::string& uuid,
                                const std::string& content_type,
                                const std::string& content_disposition,
                                std::vector<mojom::DataElementPtr> elements,
                                RegisterCallback callback) {
  if (uuid.empty() || context_->registry().HasEntry(uuid)) {
    bindings_.ReportBadMessage("Invalid UUID passed to BlobRegistry::Register");
    return;
  }

  // TODO(mek): Actually register the blob.
  std::unique_ptr<BlobDataHandle> handle =
      context_->AddBrokenBlob(uuid, content_type, content_disposition,
                              BlobStatus::ERR_SOURCE_DIED_IN_TRANSIT);
  BlobImpl::Create(std::move(handle), std::move(blob));
  std::move(callback).Run();
}

void BlobRegistryImpl::GetBlobFromUUID(mojom::BlobRequest blob,
                                       const std::string& uuid) {
  if (uuid.empty()) {
    bindings_.ReportBadMessage(
        "Invalid UUID passed to BlobRegistry::GetBlobFromUUID");
    return;
  }
  if (!context_->registry().HasEntry(uuid)) {
    // TODO(mek): Log histogram, old code logs Storage.Blob.InvalidReference
    return;
  }
  BlobImpl::Create(context_->GetBlobDataFromUUID(uuid), std::move(blob));
}

}  // namespace storage
