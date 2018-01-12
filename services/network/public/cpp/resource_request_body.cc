// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/resource_request_body.h"

namespace network {

ResourceRequestBody::ResourceRequestBody()
    : identifier_(0), contains_sensitive_info_(false) {}

// static
scoped_refptr<ResourceRequestBody> ResourceRequestBody::CreateFromBytes(
    const char* bytes,
    size_t length) {
  scoped_refptr<ResourceRequestBody> result = new ResourceRequestBody();
  result->AppendBytes(bytes, length);
  return result;
}

void ResourceRequestBody::AppendBytes(const char* bytes, int bytes_len) {
  if (bytes_len > 0) {
    elements_.push_back(DataElement());
    elements_.back().SetToBytes(bytes, bytes_len);
  }
}

void ResourceRequestBody::AppendFileRange(
    const base::FilePath& file_path,
    uint64_t offset,
    uint64_t length,
    const base::Time& expected_modification_time) {
  elements_.push_back(DataElement());
  elements_.back().SetToFilePathRange(file_path, offset, length,
                                      expected_modification_time);
}

void ResourceRequestBody::AppendRawFileRange(
    base::File file,
    const base::FilePath& file_path,
    uint64_t offset,
    uint64_t length,
    const base::Time& expected_modification_time) {
  elements_.push_back(DataElement());
  elements_.back().SetToFileRange(std::move(file), file_path, offset, length,
                                  expected_modification_time);
}

void ResourceRequestBody::AppendBlob(const std::string& uuid) {
  elements_.push_back(DataElement());
  elements_.back().SetToBlob(uuid);
}

void ResourceRequestBody::AppendFileSystemFileRange(
    const GURL& url,
    uint64_t offset,
    uint64_t length,
    const base::Time& expected_modification_time) {
  elements_.push_back(DataElement());
  elements_.back().SetToFileSystemUrlRange(url, offset, length,
                                           expected_modification_time);
}

void ResourceRequestBody::AppendDataPipe(
    mojom::DataPipeGetterPtr data_pipe_getter) {
  elements_.push_back(DataElement());
  elements_.back().SetToDataPipe(std::move(data_pipe_getter));
}

std::vector<base::FilePath> ResourceRequestBody::GetReferencedFiles() const {
  std::vector<base::FilePath> result;
  for (const auto& element : *elements()) {
    if (element.type() == DataElement::TYPE_FILE)
      result.push_back(element.path());
  }
  return result;
}

ResourceRequestBody::~ResourceRequestBody() {}

}  // namespace network
