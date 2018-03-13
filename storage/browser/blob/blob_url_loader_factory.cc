// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_url_loader_factory.h"

#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_url_loader.h"

namespace storage {

// static
void BlobURLLoaderFactory::Create(
    std::unique_ptr<BlobDataHandle> handle,
    const GURL& blob_url,
    network::mojom::URLLoaderFactoryRequest request) {
  new BlobURLLoaderFactory(std::move(handle), blob_url, std::move(request));
}

void BlobURLLoaderFactory::CreateLoaderAndStart(
    network::mojom::URLLoaderRequest loader,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    network::mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  if (request.url != url_) {
    bindings_.ReportBadMessage("Invalid URL when attempting to fetch Blob");
    return;
  }
  DCHECK(!request.download_to_file);
  BlobURLLoader::CreateAndStart(
      std::move(loader), request, std::move(client),
      handle_ ? std::make_unique<BlobDataHandle>(*handle_) : nullptr);
}

void BlobURLLoaderFactory::Clone(
    network::mojom::URLLoaderFactoryRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

BlobURLLoaderFactory::BlobURLLoaderFactory(
    std::unique_ptr<BlobDataHandle> handle,
    const GURL& blob_url,
    network::mojom::URLLoaderFactoryRequest request)
    : handle_(std::move(handle)), url_(blob_url) {
  bindings_.AddBinding(this, std::move(request));
  bindings_.set_connection_error_handler(base::BindRepeating(
      &BlobURLLoaderFactory::OnConnectionError, base::Unretained(this)));
}

BlobURLLoaderFactory::~BlobURLLoaderFactory() = default;

void BlobURLLoaderFactory::OnConnectionError() {
  if (!bindings_.empty())
    return;
  delete this;
}

}  // namespace storage
