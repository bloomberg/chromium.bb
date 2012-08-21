// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/resource_request_body.h"

#include "base/logging.h"
#include "net/base/upload_data.h"

namespace webkit_glue {

ResourceRequestBody::Element::Element()
    : type_(TYPE_BYTES),
      bytes_start_(NULL),
      bytes_length_(0),
      file_range_offset_(0),
      file_range_length_(kuint64max) {
}

ResourceRequestBody::Element::~Element() {}

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

net::UploadData* ResourceRequestBody::CreateUploadData() {
  net::UploadData* upload_data = new net::UploadData;
  // We attach 'this' to UploadData so that we do not need to copy
  // bytes for TYPE_BYTES.
  upload_data->SetUserData(
      this, new base::UserDataAdapter<ResourceRequestBody>(this));
  std::vector<net::UploadElement>* uploads =
      upload_data->elements_mutable();
  for (size_t i = 0; i < elements_.size(); ++i) {
    const Element& element = elements_[i];
    if (element.type() == TYPE_BYTES) {
      uploads->push_back(net::UploadElement());
      uploads->back().SetToSharedBytes(element.bytes(),
                                       element.bytes_length());
      continue;
    }

    DCHECK(element.type() == TYPE_FILE);
    uploads->push_back(net::UploadElement());
    uploads->back().SetToFilePathRange(
        element.file_path(),
        element.file_range_offset(),
        element.file_range_length(),
        element.expected_file_modification_time());
  }
  upload_data->set_identifier(identifier_);
  return upload_data;
}

ResourceRequestBody::~ResourceRequestBody() {}

}  // namespace webkit_glue
