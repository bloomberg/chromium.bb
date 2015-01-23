// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_storage_context.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "url/gurl.h"

namespace storage {

namespace {

// We can't use GURL directly for these hash fragment manipulations
// since it doesn't have specific knowlege of the BlobURL format. GURL
// treats BlobURLs as if they were PathURLs which don't support hash
// fragments.

bool BlobUrlHasRef(const GURL& url) {
  return url.spec().find('#') != std::string::npos;
}

GURL ClearBlobUrlRef(const GURL& url) {
  size_t hash_pos = url.spec().find('#');
  if (hash_pos == std::string::npos)
    return url;
  return GURL(url.spec().substr(0, hash_pos));
}

// TODO(michaeln): use base::SysInfo::AmountOfPhysicalMemoryMB() in some
// way to come up with a better limit.
static const int64 kMaxMemoryUsage = 500 * 1024 * 1024;  // Half a gig.

}  // namespace

BlobStorageContext::BlobMapEntry::BlobMapEntry()
    : refcount(0), flags(0) {
}

BlobStorageContext::BlobMapEntry::BlobMapEntry(int refcount,
                                               BlobDataBuilder* data)
    : refcount(refcount), flags(0), data_builder(data) {
}

BlobStorageContext::BlobMapEntry::~BlobMapEntry() {
}

bool BlobStorageContext::BlobMapEntry::IsBeingBuilt() {
  return data_builder;
}

BlobStorageContext::BlobStorageContext()
    : memory_usage_(0) {
}

BlobStorageContext::~BlobStorageContext() {
  STLDeleteContainerPairSecondPointers(blob_map_.begin(), blob_map_.end());
}

scoped_ptr<BlobDataHandle> BlobStorageContext::GetBlobDataFromUUID(
    const std::string& uuid) {
  scoped_ptr<BlobDataHandle> result;
  BlobMap::iterator found = blob_map_.find(uuid);
  if (found == blob_map_.end())
    return result.Pass();
  auto* entry = found->second;
  if (entry->flags & EXCEEDED_MEMORY)
    return result.Pass();
  DCHECK(!entry->IsBeingBuilt());
  result.reset(
      new BlobDataHandle(uuid, this, base::MessageLoopProxy::current().get()));
  return result.Pass();
}

scoped_ptr<BlobDataHandle> BlobStorageContext::GetBlobDataFromPublicURL(
    const GURL& url) {
  BlobURLMap::iterator found = public_blob_urls_.find(
      BlobUrlHasRef(url) ? ClearBlobUrlRef(url) : url);
  if (found == public_blob_urls_.end())
    return scoped_ptr<BlobDataHandle>();
  return GetBlobDataFromUUID(found->second);
}

scoped_ptr<BlobDataHandle> BlobStorageContext::AddFinishedBlob(
    const BlobDataBuilder& data) {
  StartBuildingBlob(data.uuid_);
  for (const auto& blob_item : data.items_)
    AppendBlobDataItem(data.uuid_, *(blob_item->item_));
  FinishBuildingBlob(data.uuid_, data.content_type_);
  scoped_ptr<BlobDataHandle> handle = GetBlobDataFromUUID(data.uuid_);
  DecrementBlobRefCount(data.uuid_);
  return handle.Pass();
}

bool BlobStorageContext::RegisterPublicBlobURL(
    const GURL& blob_url, const std::string& uuid) {
  DCHECK(!BlobUrlHasRef(blob_url));
  DCHECK(IsInUse(uuid));
  DCHECK(!IsUrlRegistered(blob_url));
  if (!IsInUse(uuid) || IsUrlRegistered(blob_url))
    return false;
  IncrementBlobRefCount(uuid);
  public_blob_urls_[blob_url] = uuid;
  return true;
}

void BlobStorageContext::RevokePublicBlobURL(const GURL& blob_url) {
  DCHECK(!BlobUrlHasRef(blob_url));
  if (!IsUrlRegistered(blob_url))
    return;
  DecrementBlobRefCount(public_blob_urls_[blob_url]);
  public_blob_urls_.erase(blob_url);
}

scoped_ptr<BlobDataSnapshot> BlobStorageContext::CreateSnapshot(
    const std::string& uuid) {
  scoped_ptr<BlobDataSnapshot> result;
  auto found = blob_map_.find(uuid);
  DCHECK(found != blob_map_.end())
      << "Blob should be in map, as the handle is still around";
  BlobMapEntry* entry = found->second;
  DCHECK(!entry->IsBeingBuilt());
  result.reset(new BlobDataSnapshot(*entry->data));
  return result.Pass();
}

void BlobStorageContext::StartBuildingBlob(const std::string& uuid) {
  DCHECK(!IsInUse(uuid) && !uuid.empty());
  blob_map_[uuid] = new BlobMapEntry(1, new BlobDataBuilder(uuid));
}

void BlobStorageContext::AppendBlobDataItem(const std::string& uuid,
                                            const DataElement& item) {
  DCHECK(IsBeingBuilt(uuid));
  BlobMap::iterator found = blob_map_.find(uuid);
  if (found == blob_map_.end())
    return;
  BlobMapEntry* entry = found->second;
  if (entry->flags & EXCEEDED_MEMORY)
    return;
  BlobDataBuilder* target_blob_data = entry->data_builder.get();
  DCHECK(target_blob_data);

  bool exceeded_memory = false;

  // The blob data is stored in the canonical way which only contains a
  // list of Data, File, and FileSystem items. Aggregated TYPE_BLOB items
  // are expanded into the primitive constituent types.
  // 1) The Data item is denoted by the raw data and length.
  // 2) The File item is denoted by the file path, the range and the expected
  //    modification time.
  // 3) The FileSystem File item is denoted by the FileSystem URL, the range
  //    and the expected modification time.
  // 4) The Blob items are expanded.
  // TODO(michaeln): Would be nice to avoid copying Data items when expanding.

  uint64 length = item.length();
  DCHECK_GT(length, 0u);
  UMA_HISTOGRAM_COUNTS("Storage.Blob.StorageSizeBeforeAppend",
                       memory_usage_ / 1024);
  switch (item.type()) {
    case DataElement::TYPE_BYTES:
      UMA_HISTOGRAM_COUNTS("Storage.BlobItemSize.Bytes", length / 1024);
      DCHECK(!item.offset());
      exceeded_memory = !AppendBytesItem(target_blob_data, item.bytes(),
                                         static_cast<int64>(length));
      break;
    case DataElement::TYPE_FILE:
      UMA_HISTOGRAM_COUNTS("Storage.BlobItemSize.File", length / 1024);
      AppendFileItem(target_blob_data, item.path(), item.offset(),
                     item.length(), item.expected_modification_time());
      break;
    case DataElement::TYPE_FILE_FILESYSTEM:
      UMA_HISTOGRAM_COUNTS("Storage.BlobItemSize.FileSystem", length / 1024);
      AppendFileSystemFileItem(target_blob_data, item.filesystem_url(),
                               item.offset(), item.length(),
                               item.expected_modification_time());
      break;
    case DataElement::TYPE_BLOB: {
      UMA_HISTOGRAM_COUNTS("Storage.BlobItemSize.Blob", length / 1024);
      // We grab the handle to ensure it stays around while we copy it.
      scoped_ptr<BlobDataHandle> src = GetBlobDataFromUUID(item.blob_uuid());
      if (src) {
        BlobMapEntry* entry = blob_map_.find(item.blob_uuid())->second;
        DCHECK(entry->data);
        exceeded_memory = !ExpandStorageItems(target_blob_data, *entry->data,
                                              item.offset(), item.length());
      }
      break;
    }
    default:
      NOTREACHED();
      break;
  }
  UMA_HISTOGRAM_COUNTS("Storage.Blob.StorageSizeAfterAppend",
                       memory_usage_ / 1024);

  // If we're using too much memory, drop this blob's data.
  // TODO(michaeln): Blob memory storage does not yet spill over to disk,
  // as a stop gap, we'll prevent memory usage over a max amount.
  if (exceeded_memory) {
    memory_usage_ -= target_blob_data->GetMemoryUsage();
    entry->flags |= EXCEEDED_MEMORY;
    entry->data_builder.reset(new BlobDataBuilder(uuid));
    return;
  }
}

void BlobStorageContext::FinishBuildingBlob(
    const std::string& uuid, const std::string& content_type) {
  DCHECK(IsBeingBuilt(uuid));
  BlobMap::iterator found = blob_map_.find(uuid);
  if (found == blob_map_.end())
    return;
  BlobMapEntry* entry = found->second;
  entry->data_builder->set_content_type(content_type);
  entry->data = entry->data_builder->BuildSnapshot().Pass();
  entry->data_builder.reset();
  UMA_HISTOGRAM_COUNTS("Storage.Blob.ItemCount", entry->data->items().size());
  UMA_HISTOGRAM_BOOLEAN("Storage.Blob.ExceededMemory",
                        (entry->flags & EXCEEDED_MEMORY) == EXCEEDED_MEMORY);
}

void BlobStorageContext::CancelBuildingBlob(const std::string& uuid) {
  DCHECK(IsBeingBuilt(uuid));
  DecrementBlobRefCount(uuid);
}

void BlobStorageContext::IncrementBlobRefCount(const std::string& uuid) {
  BlobMap::iterator found = blob_map_.find(uuid);
  if (found == blob_map_.end()) {
    DCHECK(false);
    return;
  }
  ++(found->second->refcount);
}

void BlobStorageContext::DecrementBlobRefCount(const std::string& uuid) {
  BlobMap::iterator found = blob_map_.find(uuid);
  if (found == blob_map_.end())
    return;
  auto* entry = found->second;
  if (--(entry->refcount) == 0) {
    if (entry->IsBeingBuilt()) {
      memory_usage_ -= entry->data_builder->GetMemoryUsage();
    } else {
      memory_usage_ -= entry->data->GetMemoryUsage();
    }
    delete entry;
    blob_map_.erase(found);
  }
}

bool BlobStorageContext::ExpandStorageItems(
    BlobDataBuilder* target_blob_data,
    const BlobDataSnapshot& src_blob_data,
    uint64 offset,
    uint64 length) {
  DCHECK(target_blob_data && length != static_cast<uint64>(-1));

  const std::vector<scoped_refptr<BlobDataItem>>& items = src_blob_data.items();
  auto iter = items.begin();
  if (offset) {
    for (; iter != items.end(); ++iter) {
      const BlobDataItem& item = *(iter->get());
      if (offset >= item.length())
        offset -= item.length();
      else
        break;
    }
  }

  for (; iter != items.end() && length > 0; ++iter) {
    const BlobDataItem& item = *(iter->get());
    uint64 current_length = item.length() - offset;
    uint64 new_length = current_length > length ? length : current_length;
    if (iter->get()->type() == DataElement::TYPE_BYTES) {
      if (!AppendBytesItem(
              target_blob_data,
              item.bytes() + static_cast<size_t>(item.offset() + offset),
              static_cast<int64>(new_length))) {
        return false;  // exceeded memory
      }
    } else if (item.type() == DataElement::TYPE_FILE) {
      AppendFileItem(target_blob_data, item.path(), item.offset() + offset,
                     new_length, item.expected_modification_time());
    } else {
      DCHECK(item.type() == DataElement::TYPE_FILE_FILESYSTEM);
      AppendFileSystemFileItem(target_blob_data, item.filesystem_url(),
                               item.offset() + offset, new_length,
                               item.expected_modification_time());
    }
    length -= new_length;
    offset = 0;
  }
  return true;
}

bool BlobStorageContext::AppendBytesItem(BlobDataBuilder* target_blob_data,
                                         const char* bytes,
                                         int64 length) {
  if (length < 0) {
    DCHECK(false);
    return false;
  }
  if (memory_usage_ + length > kMaxMemoryUsage) {
    return false;
  }
  target_blob_data->AppendData(bytes, static_cast<size_t>(length));
  memory_usage_ += length;
  return true;
}

void BlobStorageContext::AppendFileItem(
    BlobDataBuilder* target_blob_data,
    const base::FilePath& file_path,
    uint64 offset,
    uint64 length,
    const base::Time& expected_modification_time) {
  // It may be a temporary file that should be deleted when no longer needed.
  scoped_refptr<ShareableFileReference> shareable_file =
      ShareableFileReference::Get(file_path);

  target_blob_data->AppendFile(file_path, offset, length,
                               expected_modification_time, shareable_file);
}

void BlobStorageContext::AppendFileSystemFileItem(
    BlobDataBuilder* target_blob_data,
    const GURL& filesystem_url,
    uint64 offset,
    uint64 length,
    const base::Time& expected_modification_time) {
  target_blob_data->AppendFileSystemFile(filesystem_url, offset, length,
                                         expected_modification_time);
}

bool BlobStorageContext::IsInUse(const std::string& uuid) {
  return blob_map_.find(uuid) != blob_map_.end();
}

bool BlobStorageContext::IsBeingBuilt(const std::string& uuid) {
  BlobMap::iterator found = blob_map_.find(uuid);
  if (found == blob_map_.end())
    return false;
  return found->second->IsBeingBuilt();
}

bool BlobStorageContext::IsUrlRegistered(const GURL& blob_url) {
  return public_blob_urls_.find(blob_url) != public_blob_urls_.end();
}

}  // namespace storage
