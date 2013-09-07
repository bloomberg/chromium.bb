// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/blob/blob_storage_host.h"

#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "url/gurl.h"
#include "webkit/browser/blob/blob_data_handle.h"
#include "webkit/browser/blob/blob_storage_context.h"

namespace webkit_blob {

BlobStorageHost::BlobStorageHost(BlobStorageContext* context)
    : context_(context->AsWeakPtr()) {
}

BlobStorageHost::~BlobStorageHost() {
  if (!context_.get())
    return;
  for (std::set<GURL>::iterator iter = public_blob_urls_.begin();
       iter != public_blob_urls_.end(); ++iter) {
    context_->RevokePublicBlobURL(*iter);
  }
  for (BlobReferenceMap::iterator iter = blobs_inuse_map_.begin();
       iter != blobs_inuse_map_.end(); ++iter) {
    for (int i = 0; i < iter->second; ++i)
      context_->DecrementBlobRefCount(iter->first);
  }
  for (std::set<GURL>::iterator iter = private_blob_urls_.begin();
       iter != private_blob_urls_.end(); ++iter) {
    context_->DeprecatedRevokePrivateBlobURL(*iter);
  }
}

bool BlobStorageHost::StartBuildingBlob(const std::string& uuid) {
  if (!context_.get() || uuid.empty() || context_->IsInUse(uuid))
    return false;
  context_->StartBuildingBlob(uuid);
  blobs_inuse_map_[uuid] = 1;
  return true;
}

bool BlobStorageHost::AppendBlobDataItem(
    const std::string& uuid, const BlobData::Item& data_item) {
  if (!context_.get() || !IsBeingBuiltInHost(uuid))
    return false;
  context_->AppendBlobDataItem(uuid, data_item);
  return true;
}

bool BlobStorageHost::CancelBuildingBlob(const std::string& uuid) {
  if (!context_.get() || !IsBeingBuiltInHost(uuid))
    return false;
  blobs_inuse_map_.erase(uuid);
  context_->CancelBuildingBlob(uuid);
  return true;
}

bool BlobStorageHost::FinishBuildingBlob(
    const std::string& uuid, const std::string& content_type) {
  if (!context_.get() || !IsBeingBuiltInHost(uuid))
    return false;
  context_->FinishBuildingBlob(uuid, content_type);
  return true;
}

bool BlobStorageHost::IncrementBlobRefCount(const std::string& uuid) {
  if (!context_.get() || !context_->IsInUse(uuid) ||
      context_->IsBeingBuilt(uuid))
    return false;
  context_->IncrementBlobRefCount(uuid);
  blobs_inuse_map_[uuid] += 1;
  return true;
}

bool BlobStorageHost::DecrementBlobRefCount(const std::string& uuid) {
  if (!context_.get() || !IsInUseInHost(uuid))
    return false;
  context_->DecrementBlobRefCount(uuid);
  blobs_inuse_map_[uuid] -= 1;
  if (blobs_inuse_map_[uuid] == 0)
    blobs_inuse_map_.erase(uuid);
  return true;
}

bool BlobStorageHost::RegisterPublicBlobURL(
    const GURL& blob_url, const std::string& uuid) {
  if (!context_.get() || !IsInUseInHost(uuid) ||
      context_->IsUrlRegistered(blob_url))
    return false;
  context_->RegisterPublicBlobURL(blob_url, uuid);
  public_blob_urls_.insert(blob_url);
  return true;
}

bool BlobStorageHost::RevokePublicBlobURL(const GURL& blob_url) {
  if (!context_.get() || !IsUrlRegisteredInHost(blob_url))
    return false;
  context_->RevokePublicBlobURL(blob_url);
  public_blob_urls_.erase(blob_url);
  return true;
}

namespace {
bool IsPrivateBlobURL(const GURL& url) {
  return StartsWithASCII(url.spec(), "blob:blobinternal", true);
}
}

void BlobStorageHost::DeprecatedRegisterBlobURL(
    const GURL& private_url, const std::string& uuid) {
  DCHECK(IsPrivateBlobURL(private_url));
  if (!context_.get())
    return;
  context_->DeprecatedRegisterPrivateBlobURL(private_url, uuid);
  private_blob_urls_.insert(private_url);
}

void BlobStorageHost::DeprecatedCloneBlobURL(
    const GURL& url, const GURL& src_private_url) {
  // This method is used in two ways.
  // 1. During serialization/deserialization to 'clone' an existing blob.
  //    In this case the src and dest urls are 'private' blob urls.
  // 2. To register public blob urls. In this case the dest url is a
  //    'public' blob url.
  DCHECK(IsPrivateBlobURL(src_private_url));
  if (!context_.get())
    return;
  std::string uuid = context_->LookupUuidFromDeprecatedURL(src_private_url);
  if (uuid.empty())
    return;
  if (IsPrivateBlobURL(url)) {
    DeprecatedRegisterBlobURL(url, uuid);
  } else {
    // Temporarily bump the refcount so the uuid passes the InUse
    // check inside the RegisterPublicBlobURL method.
    ignore_result(IncrementBlobRefCount(uuid));
    ignore_result(RegisterPublicBlobURL(url, uuid));
    ignore_result(DecrementBlobRefCount(uuid));
  }
}

void BlobStorageHost::DeprecatedRevokeBlobURL(const GURL& url) {
  if (!context_.get())
    return;
  if (IsPrivateBlobURL(url)) {
    context_->DeprecatedRevokePrivateBlobURL(url);
    private_blob_urls_.erase(url);
  } else {
    ignore_result(RevokePublicBlobURL(url));
  }
}

bool BlobStorageHost::IsInUseInHost(const std::string& uuid) {
  return blobs_inuse_map_.find(uuid) != blobs_inuse_map_.end();
}

bool BlobStorageHost::IsBeingBuiltInHost(const std::string& uuid) {
  return IsInUseInHost(uuid) && context_->IsBeingBuilt(uuid);
}

bool BlobStorageHost::IsUrlRegisteredInHost(const GURL& blob_url) {
  return public_blob_urls_.find(blob_url) != public_blob_urls_.end();
}

}  // namespace webkit_blob
