// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/common/data_element.h"

#include <algorithm>

#include "base/strings/string_number_conversions.h"

namespace storage {

DataElement::DataElement()
    : type_(TYPE_UNKNOWN),
      bytes_(NULL),
      offset_(0),
      length_(kuint64max) {
}

DataElement::~DataElement() {}

void DataElement::SetToFilePathRange(
    const base::FilePath& path,
    uint64 offset, uint64 length,
    const base::Time& expected_modification_time) {
  type_ = TYPE_FILE;
  path_ = path;
  offset_ = offset;
  length_ = length;
  expected_modification_time_ = expected_modification_time;
}

void DataElement::SetToBlobRange(
    const std::string& blob_uuid,
    uint64 offset, uint64 length) {
  type_ = TYPE_BLOB;
  blob_uuid_ = blob_uuid;
  offset_ = offset;
  length_ = length;
}

void DataElement::SetToFileSystemUrlRange(
    const GURL& filesystem_url,
    uint64 offset, uint64 length,
    const base::Time& expected_modification_time) {
  type_ = TYPE_FILE_FILESYSTEM;
  filesystem_url_ = filesystem_url;
  offset_ = offset;
  length_ = length;
  expected_modification_time_ = expected_modification_time;
}

void DataElement::SetToDiskCacheEntryRange(uint64 offset, uint64 length) {
  type_ = TYPE_DISK_CACHE_ENTRY;
  offset_ = offset;
  length_ = length;
}

void PrintTo(const DataElement& x, std::ostream* os) {
  const uint64 kMaxDataPrintLength = 40;
  *os << "<DataElement>{type: ";
  switch (x.type()) {
    case DataElement::TYPE_BYTES: {
      uint64 length = std::min(x.length(), kMaxDataPrintLength);
      *os << "TYPE_BYTES, data: ["
          << base::HexEncode(x.bytes(), static_cast<size_t>(length));
      if (length < x.length()) {
        *os << "<...truncated due to length...>";
      }
      *os << "]";
      break;
    }
    case DataElement::TYPE_FILE:
      *os << "TYPE_FILE, path: " << x.path().AsUTF8Unsafe()
          << ", expected_modification_time: " << x.expected_modification_time();
      break;
    case DataElement::TYPE_BLOB:
      *os << "TYPE_BLOB, uuid: " << x.blob_uuid();
      break;
    case DataElement::TYPE_FILE_FILESYSTEM:
      *os << "TYPE_FILE_FILESYSTEM, filesystem_url: " << x.filesystem_url();
      break;
    case DataElement::TYPE_DISK_CACHE_ENTRY:
      *os << "TYPE_DISK_CACHE_ENTRY";
      break;
    case DataElement::TYPE_BYTES_DESCRIPTION:
      *os << "TYPE_BYTES_DESCRIPTION";
      break;
    case DataElement::TYPE_UNKNOWN:
      *os << "TYPE_UNKNOWN";
      break;
  }
  *os << ", length: " << x.length() << ", offset: " << x.offset() << "}";
}

}  // namespace storage
