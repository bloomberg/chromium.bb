// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_data_builder.h"

#include "base/time/time.h"
#include "storage/browser/blob/shareable_file_reference.h"

namespace storage {

BlobDataBuilder::BlobDataBuilder(const std::string& uuid) : uuid_(uuid) {
}
BlobDataBuilder::~BlobDataBuilder() {
}

void BlobDataBuilder::AppendData(const char* data, size_t length) {
  if (!length)
    return;
  scoped_ptr<DataElement> element(new DataElement());
  element->SetToBytes(data, length);
  items_.push_back(new BlobDataItem(element.Pass()));
}

void BlobDataBuilder::AppendFile(const base::FilePath& file_path,
                                 uint64_t offset,
                                 uint64_t length,
                                 const base::Time& expected_modification_time) {
  scoped_ptr<DataElement> element(new DataElement());
  element->SetToFilePathRange(file_path, offset, length,
                              expected_modification_time);
  items_.push_back(
      new BlobDataItem(element.Pass(), ShareableFileReference::Get(file_path)));
}

void BlobDataBuilder::AppendBlob(const std::string& uuid,
                                 uint64_t offset,
                                 uint64_t length) {
  DCHECK_GT(length, 0ul);
  scoped_ptr<DataElement> element(new DataElement());
  element->SetToBlobRange(uuid, offset, length);
  items_.push_back(new BlobDataItem(element.Pass()));
}

void BlobDataBuilder::AppendBlob(const std::string& uuid) {
  scoped_ptr<DataElement> element(new DataElement());
  element->SetToBlob(uuid);
  items_.push_back(new BlobDataItem(element.Pass()));
}

void BlobDataBuilder::AppendFileSystemFile(
    const GURL& url,
    uint64_t offset,
    uint64_t length,
    const base::Time& expected_modification_time) {
  DCHECK(length > 0);
  scoped_ptr<DataElement> element(new DataElement());
  element->SetToFileSystemUrlRange(url, offset, length,
                                   expected_modification_time);
  items_.push_back(new BlobDataItem(element.Pass()));
}

}  // namespace storage
