// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/resource_request_body.h"

#include "base/logging.h"
#include "net/base/upload_data.h"
#include "webkit/blob/blob_storage_controller.h"

using webkit_blob::BlobData;
using webkit_blob::BlobStorageController;

namespace webkit_glue {

ResourceRequestBody::ResourceRequestBody() : identifier_(0) {}

void ResourceRequestBody::AppendBytes(const char* bytes, int bytes_len) {
  if (bytes_len > 0) {
    elements_.push_back(Element());
    elements_.back().SetToBytes(bytes, bytes_len);
  }
}

void ResourceRequestBody::AppendFileRange(
    const FilePath& file_path,
    uint64 offset, uint64 length,
    const base::Time& expected_modification_time) {
  elements_.push_back(Element());
  elements_.back().SetToFilePathRange(file_path, offset, length,
                                      expected_modification_time);
}

void ResourceRequestBody::AppendBlob(const GURL& blob_url) {
  elements_.push_back(Element());
  elements_.back().SetToBlobUrl(blob_url);
}

void ResourceRequestBody::AppendFileSystemFileRange(
    const GURL& url, uint64 offset, uint64 length,
    const base::Time& expected_modification_time) {
  elements_.push_back(Element());
  elements_.back().SetToFileSystemUrlRange(url, offset, length,
                                           expected_modification_time);
}

net::UploadData* ResourceRequestBody::ResolveElementsAndCreateUploadData(
    BlobStorageController* blob_controller) {
  // Resolve all blob elements.
  std::vector<const Element*> resolved_elements;
  for (size_t i = 0; i < elements_.size(); ++i) {
    const Element& element = elements_[i];
    if (element.type() == Element::TYPE_BLOB) {
      ResolveBlobReference(blob_controller, element.url(), &resolved_elements);
    } else {
      // No need to resolve, just append the element.
      resolved_elements.push_back(&element);
    }
  }

  net::UploadData* upload_data = new net::UploadData;
  // We attach 'this' to UploadData so that we do not need to copy
  // bytes for TYPE_BYTES.
  upload_data->SetUserData(
      this, new base::UserDataAdapter<ResourceRequestBody>(this));
  std::vector<net::UploadElement>* elements =
      upload_data->elements_mutable();
  for (size_t i = 0; i < resolved_elements.size(); ++i) {
    const Element& element = *resolved_elements[i];
    switch (element.type()) {
      case Element::TYPE_BYTES:
        elements->push_back(net::UploadElement());
        elements->back().SetToSharedBytes(element.bytes(), element.length());
        break;
      case Element::TYPE_FILE:
        elements->push_back(net::UploadElement());
        elements->back().SetToFilePathRange(
            element.path(),
            element.offset(),
            element.length(),
            element.expected_modification_time());
        break;
      case Element::TYPE_FILE_FILESYSTEM:
        // TODO(kinuko): Resolve FileSystemURL before creating UploadData.
        NOTREACHED();
        break;
      case Element::TYPE_BLOB:
        // Blob elements should be resolved beforehand.
        NOTREACHED();
        break;
      case Element::TYPE_UNKNOWN:
        NOTREACHED();
        break;
    }
  }
  upload_data->set_identifier(identifier_);
  return upload_data;
}

ResourceRequestBody::~ResourceRequestBody() {}

void ResourceRequestBody::ResolveBlobReference(
    webkit_blob::BlobStorageController* blob_controller,
    const GURL& blob_url,
    std::vector<const Element*>* resolved_elements) {
  DCHECK(blob_controller);
  BlobData* blob_data = blob_controller->GetBlobDataFromUrl(blob_url);
  DCHECK(blob_data);
  if (!blob_data)
    return;

  // If there is no element in the referred blob data, just return.
  if (blob_data->items().empty())
    return;

  // Ensure the blob and any attached shareable files survive until
  // upload completion.
  SetUserData(blob_data, new base::UserDataAdapter<BlobData>(blob_data));

  // Append the elements in the referred blob data.
  for (size_t i = 0; i < blob_data->items().size(); ++i) {
    const BlobData::Item& item = blob_data->items().at(i);
    DCHECK_NE(BlobData::Item::TYPE_BLOB, item.type());
    resolved_elements->push_back(&item);
  }
}

}  // namespace webkit_glue
