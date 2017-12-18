// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_url_store_impl.h"

#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"

namespace storage {

BlobURLStoreImpl::BlobURLStoreImpl(base::WeakPtr<BlobStorageContext> context,
                                   BlobRegistryImpl::Delegate* delegate)
    : context_(std::move(context)),
      delegate_(delegate),
      weak_ptr_factory_(this) {}

BlobURLStoreImpl::~BlobURLStoreImpl() {
  if (context_) {
    for (const auto& url : urls_)
      context_->RevokePublicBlobURL(url);
  }
}

void BlobURLStoreImpl::Register(blink::mojom::BlobPtr blob,
                                const GURL& url,
                                RegisterCallback callback) {
  if (!url.SchemeIsBlob() || !delegate_->CanCommitURL(url)) {
    mojo::ReportBadMessage("Invalid Blob URL passed to BlobURLStore::Register");
    std::move(callback).Run();
    return;
  }

  blink::mojom::Blob* blob_ptr = blob.get();
  blob_ptr->GetInternalUUID(base::BindOnce(
      &BlobURLStoreImpl::RegisterWithUUID, weak_ptr_factory_.GetWeakPtr(),
      std::move(blob), url, std::move(callback)));
}

void BlobURLStoreImpl::Revoke(const GURL& url) {
  if (context_)
    context_->RevokePublicBlobURL(url);
  urls_.erase(url);
}

void BlobURLStoreImpl::Resolve(const GURL& url, ResolveCallback callback) {
  if (!context_) {
    std::move(callback).Run(nullptr);
    return;
  }
  blink::mojom::BlobPtr blob;
  std::unique_ptr<BlobDataHandle> blob_handle =
      context_->GetBlobDataFromPublicURL(url);
  if (blob_handle)
    BlobImpl::Create(std::move(blob_handle), MakeRequest(&blob));
  std::move(callback).Run(std::move(blob));
}

void BlobURLStoreImpl::RegisterWithUUID(blink::mojom::BlobPtr blob,
                                        const GURL& url,
                                        RegisterCallback callback,
                                        const std::string& uuid) {
  // |blob| is unused, but is passed here to be kept alive until
  // RegisterPublicBlobURL increments the refcount of it via the uuid.
  if (context_)
    context_->RegisterPublicBlobURL(url, uuid);
  urls_.insert(url);
  std::move(callback).Run();
}

}  // namespace storage
