// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_data_builder.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/files/file.h"
#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "net/disk_cache/disk_cache.h"

using base::FilePath;

namespace storage {

namespace {

const static int kInvalidDiskCacheSideStreamIndex = -1;
const FilePath::CharType kFutureFileName[] = FILE_PATH_LITERAL("_future_name_");

}  // namespace

BlobDataBuilder::FutureData::FutureData(FutureData&&) = default;
BlobDataBuilder::FutureData& BlobDataBuilder::FutureData::operator=(
    FutureData&&) = default;
BlobDataBuilder::FutureData::~FutureData() = default;

bool BlobDataBuilder::FutureData::Populate(base::span<const char> data,
                                           size_t offset) const {
  DCHECK(data.data());
  base::span<char> target = GetDataToPopulate(offset, data.length());
  if (!target.data())
    return false;
  DCHECK_EQ(target.length(), data.length());
  std::memcpy(target.data(), data.data(), data.length());
  return true;
}

base::span<char> BlobDataBuilder::FutureData::GetDataToPopulate(
    size_t offset,
    size_t length) const {
  network::DataElement* element = item_->data_element_ptr();

  // We lazily allocate our data buffer by waiting until the first
  // PopulateFutureData call.
  // Why? The reason we have the AppendFutureData  method is to create our Blob
  // record when the Renderer tells us about the blob without actually
  // allocating the memory yet, as we might not have the quota yet. So we don't
  // want to allocate the memory until we're actually receiving the data (which
  // the browser process only does when it has quota).
  if (element->type() == network::DataElement::TYPE_BYTES_DESCRIPTION) {
    element->SetToAllocatedBytes(element->length());
    // The type of the element is now TYPE_BYTES.
  }
  DCHECK_EQ(element->type(), network::DataElement::TYPE_BYTES);
  base::CheckedNumeric<size_t> checked_end = offset;
  checked_end += length;
  if (!checked_end.IsValid() || checked_end.ValueOrDie() > element->length()) {
    DVLOG(1) << "Invalid offset or length.";
    return nullptr;
  }
  return base::make_span(element->mutable_bytes() + offset, length);
}

BlobDataBuilder::FutureData::FutureData(scoped_refptr<BlobDataItem> item)
    : item_(item) {}

BlobDataBuilder::FutureFile::FutureFile(FutureFile&&) = default;
BlobDataBuilder::FutureFile& BlobDataBuilder::FutureFile::operator=(
    FutureFile&&) = default;
BlobDataBuilder::FutureFile::~FutureFile() = default;

bool BlobDataBuilder::FutureFile::Populate(
    scoped_refptr<ShareableFileReference> file_reference,
    const base::Time& expected_modification_time) {
  if (!item_) {
    DVLOG(1) << "File item already populated";
    return false;
  }
  network::DataElement* element = item_->data_element_ptr();
  DCHECK_EQ(element->type(), network::DataElement::TYPE_FILE);
  uint64_t length = element->length();
  uint64_t offset = element->offset();
  element->SetToFilePathRange(file_reference->path(), offset, length,
                              expected_modification_time);
  item_->data_handle_ = std::move(file_reference);
  item_ = nullptr;
  return true;
}

BlobDataBuilder::FutureFile::FutureFile(scoped_refptr<BlobDataItem> item)
    : item_(item) {}

/* static */
base::FilePath BlobDataBuilder::GetFutureFileItemPath(uint64_t file_id) {
  std::string file_id_str = base::NumberToString(file_id);
  return base::FilePath(kFutureFileName)
      .AddExtension(
          base::FilePath::StringType(file_id_str.begin(), file_id_str.end()));
}

/* static */
bool BlobDataBuilder::IsFutureFileItem(const network::DataElement& element) {
  const FilePath::StringType prefix(kFutureFileName);
  // The prefix shouldn't occur unless the user used "AppendFutureFile". We
  // DCHECK on AppendFile to make sure no one appends a future file.
  return base::StartsWith(element.path().value(), prefix,
                          base::CompareCase::SENSITIVE);
}

/* static */
uint64_t BlobDataBuilder::GetFutureFileID(const network::DataElement& element) {
  DCHECK(IsFutureFileItem(element));
  uint64_t id = 0;
  bool success =
      base::StringToUint64(element.path().Extension().substr(1), &id);
  DCHECK(success) << element.path().Extension();
  return id;
}

BlobDataBuilder::BlobDataBuilder(const std::string& uuid) : uuid_(uuid) {}

BlobDataBuilder::BlobDataBuilder(BlobDataBuilder&&) = default;
BlobDataBuilder& BlobDataBuilder::operator=(BlobDataBuilder&&) = default;
BlobDataBuilder::~BlobDataBuilder() = default;

void BlobDataBuilder::AppendIPCDataElement(
    const network::DataElement& ipc_data,
    const scoped_refptr<FileSystemContext>& file_system_context) {
  uint64_t length = ipc_data.length();
  switch (ipc_data.type()) {
    case network::DataElement::TYPE_BYTES:
      DCHECK(!ipc_data.offset());
      AppendData(ipc_data.bytes(),
                 base::checked_cast<size_t>(length));
      break;
    case network::DataElement::TYPE_FILE:
      AppendFile(ipc_data.path(), ipc_data.offset(), length,
                 ipc_data.expected_modification_time());
      break;
    case network::DataElement::TYPE_FILE_FILESYSTEM:
      AppendFileSystemFile(ipc_data.filesystem_url(), ipc_data.offset(), length,
                           ipc_data.expected_modification_time(),
                           file_system_context);
      break;
    case network::DataElement::TYPE_BLOB:
      // This is a temporary item that will be deconstructed later in
      // BlobStorageContext.
      AppendBlob(ipc_data.blob_uuid(), ipc_data.offset(), ipc_data.length());
      break;
    case network::DataElement::TYPE_RAW_FILE:
    case network::DataElement::TYPE_BYTES_DESCRIPTION:
    case network::DataElement::TYPE_UNKNOWN:
    // This type can't be sent by IPC.
    case network::DataElement::TYPE_DISK_CACHE_ENTRY:
    case network::DataElement::TYPE_DATA_PIPE:
      NOTREACHED();
      break;
  }
}

void BlobDataBuilder::AppendData(const char* data, size_t length) {
  if (!length)
    return;
  std::unique_ptr<network::DataElement> element(new network::DataElement());
  element->SetToBytes(data, length);
  items_.push_back(new BlobDataItem(std::move(element)));
}

BlobDataBuilder::FutureData BlobDataBuilder::AppendFutureData(size_t length) {
  CHECK_NE(length, 0u);
  std::unique_ptr<network::DataElement> element(new network::DataElement());
  element->SetToBytesDescription(length);
  items_.push_back(new BlobDataItem(std::move(element)));
  return FutureData(items_.back());
}

BlobDataBuilder::FutureFile BlobDataBuilder::AppendFutureFile(
    uint64_t offset,
    uint64_t length,
    uint64_t file_id) {
  CHECK_NE(length, 0ull);
  std::unique_ptr<network::DataElement> element(new network::DataElement());
  element->SetToFilePathRange(GetFutureFileItemPath(file_id), offset, length,
                              base::Time());
  items_.push_back(new BlobDataItem(std::move(element)));
  return FutureFile(items_.back());
}

void BlobDataBuilder::AppendFile(const FilePath& file_path,
                                 uint64_t offset,
                                 uint64_t length,
                                 const base::Time& expected_modification_time) {
  std::unique_ptr<network::DataElement> element(new network::DataElement());
  element->SetToFilePathRange(file_path, offset, length,
                              expected_modification_time);
  DCHECK(!IsFutureFileItem(*element)) << file_path.value();
  items_.push_back(new BlobDataItem(std::move(element),
                                    ShareableFileReference::Get(file_path)));
}

void BlobDataBuilder::AppendBlob(const std::string& uuid,
                                 uint64_t offset,
                                 uint64_t length) {
  DCHECK_GT(length, 0ul);
  std::unique_ptr<network::DataElement> element(new network::DataElement());
  element->SetToBlobRange(uuid, offset, length);
  items_.push_back(new BlobDataItem(std::move(element)));
}

void BlobDataBuilder::AppendBlob(const std::string& uuid) {
  std::unique_ptr<network::DataElement> element(new network::DataElement());
  element->SetToBlob(uuid);
  items_.push_back(new BlobDataItem(std::move(element)));
}

void BlobDataBuilder::AppendFileSystemFile(
    const GURL& url,
    uint64_t offset,
    uint64_t length,
    const base::Time& expected_modification_time,
    scoped_refptr<FileSystemContext> file_system_context) {
  DCHECK_GT(length, 0ul);
  std::unique_ptr<network::DataElement> element(new network::DataElement());
  element->SetToFileSystemUrlRange(url, offset, length,
                                   expected_modification_time);
  items_.push_back(
      new BlobDataItem(std::move(element), std::move(file_system_context)));
}

void BlobDataBuilder::AppendDiskCacheEntry(
    const scoped_refptr<DataHandle>& data_handle,
    disk_cache::Entry* disk_cache_entry,
    int disk_cache_stream_index) {
  std::unique_ptr<network::DataElement> element(new network::DataElement());
  element->SetToDiskCacheEntryRange(
      0U, disk_cache_entry->GetDataSize(disk_cache_stream_index));
  items_.push_back(new BlobDataItem(std::move(element), data_handle,
                                    disk_cache_entry, disk_cache_stream_index,
                                    kInvalidDiskCacheSideStreamIndex));
}

void BlobDataBuilder::AppendDiskCacheEntryWithSideData(
    const scoped_refptr<DataHandle>& data_handle,
    disk_cache::Entry* disk_cache_entry,
    int disk_cache_stream_index,
    int disk_cache_side_stream_index) {
  std::unique_ptr<network::DataElement> element(new network::DataElement());
  element->SetToDiskCacheEntryRange(
      0U, disk_cache_entry->GetDataSize(disk_cache_stream_index));
  items_.push_back(new BlobDataItem(std::move(element), data_handle,
                                    disk_cache_entry, disk_cache_stream_index,
                                    disk_cache_side_stream_index));
}

void BlobDataBuilder::Clear() {
  items_.clear();
  content_disposition_.clear();
  content_type_.clear();
  uuid_.clear();
}

void PrintTo(const BlobDataBuilder& x, std::ostream* os) {
  DCHECK(os);
  *os << "<BlobDataBuilder>{uuid: " << x.uuid()
      << ", content_type: " << x.content_type_
      << ", content_disposition: " << x.content_disposition_ << ", items: [";
  for (const auto& item : x.items_) {
    PrintTo(*item, os);
    *os << ", ";
  }
  *os << "]}";
}

}  // namespace storage
