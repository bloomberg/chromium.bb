// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/resource_request_body.h"

#include "base/logging.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/base/upload_file_element_reader.h"
#include "webkit/blob/blob_storage_controller.h"

using webkit_blob::BlobData;
using webkit_blob::BlobStorageController;

namespace webkit_glue {

namespace {

// A subclass of net::UploadBytesElementReader which owns ResourceRequestBody.
class BytesElementReader : public net::UploadBytesElementReader {
 public:
  BytesElementReader(ResourceRequestBody* resource_request_body,
                     const ResourceRequestBody::Element& element)
      : net::UploadBytesElementReader(element.bytes(), element.length()),
        resource_request_body_(resource_request_body) {
    DCHECK_EQ(ResourceRequestBody::Element::TYPE_BYTES, element.type());
  }

  virtual ~BytesElementReader() {}

 private:
  scoped_refptr<ResourceRequestBody> resource_request_body_;

  DISALLOW_COPY_AND_ASSIGN(BytesElementReader);
};

// A subclass of net::UploadFileElementReader which owns ResourceRequestBody.
// This class is necessary to ensure the BlobData and any attached shareable
// files survive until upload completion.
class FileElementReader : public net::UploadFileElementReader {
 public:
  FileElementReader(ResourceRequestBody* resource_request_body,
                    base::TaskRunner* task_runner,
                    const ResourceRequestBody::Element& element)
      : net::UploadFileElementReader(task_runner,
                                     element.path(),
                                     element.offset(),
                                     element.length(),
                                     element.expected_modification_time()),
        resource_request_body_(resource_request_body) {
    DCHECK_EQ(ResourceRequestBody::Element::TYPE_FILE, element.type());
  }

  virtual ~FileElementReader() {}

 private:
  scoped_refptr<ResourceRequestBody> resource_request_body_;

  DISALLOW_COPY_AND_ASSIGN(FileElementReader);
};

}  // namespace

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

net::UploadDataStream*
ResourceRequestBody::ResolveElementsAndCreateUploadDataStream(
    BlobStorageController* blob_controller,
    base::TaskRunner* task_runner) {
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

  ScopedVector<net::UploadElementReader> element_readers;
  for (size_t i = 0; i < resolved_elements.size(); ++i) {
    const Element& element = *resolved_elements[i];
    switch (element.type()) {
      case Element::TYPE_BYTES:
        element_readers.push_back(new BytesElementReader(this, element));
        break;
      case Element::TYPE_FILE:
        element_readers.push_back(
            new FileElementReader(this, task_runner, element));
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
  return new net::UploadDataStream(&element_readers, identifier_);
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
