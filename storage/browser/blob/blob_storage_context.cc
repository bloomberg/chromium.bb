// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_storage_context.h"

#include <stddef.h>
#include <stdint.h>
#include <algorithm>
#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/shareable_file_reference.h"
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
static const int64_t kMaxMemoryUsage = 500 * 1024 * 1024;  // Half a gig.

}  // namespace

BlobStorageContext::BlobMapEntry::BlobMapEntry() : refcount(0), flags(0) {
}

BlobStorageContext::BlobMapEntry::BlobMapEntry(int refcount,
                                               InternalBlobData::Builder* data)
    : refcount(refcount), flags(0), data_builder(data) {
}

BlobStorageContext::BlobMapEntry::~BlobMapEntry() {
}

bool BlobStorageContext::BlobMapEntry::IsBeingBuilt() {
  return data_builder;
}

BlobStorageContext::BlobStorageContext() : memory_usage_(0) {
}

BlobStorageContext::~BlobStorageContext() {
  STLDeleteContainerPairSecondPointers(blob_map_.begin(), blob_map_.end());
}

scoped_ptr<BlobDataHandle> BlobStorageContext::GetBlobDataFromUUID(
    const std::string& uuid) {
  scoped_ptr<BlobDataHandle> result;
  BlobMap::iterator found = blob_map_.find(uuid);
  if (found == blob_map_.end())
    return result;
  auto* entry = found->second;
  if (entry->flags & EXCEEDED_MEMORY)
    return result;
  DCHECK(!entry->IsBeingBuilt());
  result.reset(new BlobDataHandle(uuid, entry->data->content_type(),
                                  entry->data->content_disposition(), this,
                                  base::ThreadTaskRunnerHandle::Get().get()));
  return result;
}

scoped_ptr<BlobDataHandle> BlobStorageContext::GetBlobDataFromPublicURL(
    const GURL& url) {
  BlobURLMap::iterator found =
      public_blob_urls_.find(BlobUrlHasRef(url) ? ClearBlobUrlRef(url) : url);
  if (found == public_blob_urls_.end())
    return scoped_ptr<BlobDataHandle>();
  return GetBlobDataFromUUID(found->second);
}

scoped_ptr<BlobDataHandle> BlobStorageContext::AddFinishedBlob(
    const BlobDataBuilder& external_builder) {
  TRACE_EVENT0("Blob", "Context::AddFinishedBlob");
  StartBuildingBlob(external_builder.uuid_);
  BlobMap::iterator found = blob_map_.find(external_builder.uuid_);
  DCHECK(found != blob_map_.end());
  BlobMapEntry* entry = found->second;
  InternalBlobData::Builder* target_blob_builder = entry->data_builder.get();
  DCHECK(target_blob_builder);

  target_blob_builder->set_content_disposition(
      external_builder.content_disposition_);
  for (const auto& blob_item : external_builder.items_) {
    if (!AppendAllocatedBlobItem(external_builder.uuid_, blob_item,
                                 target_blob_builder)) {
      BlobEntryExceededMemory(entry);
      break;
    }
  }

  FinishBuildingBlob(external_builder.uuid_, external_builder.content_type_);
  scoped_ptr<BlobDataHandle> handle =
      GetBlobDataFromUUID(external_builder.uuid_);
  DecrementBlobRefCount(external_builder.uuid_);
  return handle;
}

scoped_ptr<BlobDataHandle> BlobStorageContext::AddFinishedBlob(
    const BlobDataBuilder* builder) {
  DCHECK(builder);
  return AddFinishedBlob(*builder);
}

bool BlobStorageContext::RegisterPublicBlobURL(const GURL& blob_url,
                                               const std::string& uuid) {
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
      << "Blob " << uuid << " should be in map, as the handle is still around";
  BlobMapEntry* entry = found->second;
  DCHECK(!entry->IsBeingBuilt());
  const InternalBlobData& data = *entry->data;

  scoped_ptr<BlobDataSnapshot> snapshot(new BlobDataSnapshot(
      uuid, data.content_type(), data.content_disposition()));
  snapshot->items_.reserve(data.items().size());
  for (const auto& shareable_item : data.items()) {
    snapshot->items_.push_back(shareable_item->item());
  }
  return snapshot;
}

void BlobStorageContext::StartBuildingBlob(const std::string& uuid) {
  DCHECK(!IsInUse(uuid) && !uuid.empty());
  blob_map_[uuid] = new BlobMapEntry(1, new InternalBlobData::Builder());
}

void BlobStorageContext::AppendBlobDataItem(
    const std::string& uuid,
    const storage::DataElement& ipc_data_element) {
  TRACE_EVENT0("Blob", "Context::AppendBlobDataItem");
  DCHECK(IsBeingBuilt(uuid));
  BlobMap::iterator found = blob_map_.find(uuid);
  if (found == blob_map_.end())
    return;
  BlobMapEntry* entry = found->second;
  if (entry->flags & EXCEEDED_MEMORY)
    return;
  InternalBlobData::Builder* target_blob_builder = entry->data_builder.get();
  DCHECK(target_blob_builder);

  if (ipc_data_element.type() == DataElement::TYPE_BYTES &&
      memory_usage_ + ipc_data_element.length() > kMaxMemoryUsage) {
    BlobEntryExceededMemory(entry);
    return;
  }
  if (!AppendAllocatedBlobItem(uuid, AllocateBlobItem(uuid, ipc_data_element),
                               target_blob_builder)) {
    BlobEntryExceededMemory(entry);
  }
}

void BlobStorageContext::FinishBuildingBlob(const std::string& uuid,
                                            const std::string& content_type) {
  DCHECK(IsBeingBuilt(uuid));
  BlobMap::iterator found = blob_map_.find(uuid);
  if (found == blob_map_.end())
    return;
  BlobMapEntry* entry = found->second;
  entry->data_builder->set_content_type(content_type);
  entry->data = entry->data_builder->Build();
  entry->data_builder.reset();
  UMA_HISTOGRAM_COUNTS("Storage.Blob.ItemCount", entry->data->items().size());
  UMA_HISTOGRAM_BOOLEAN("Storage.Blob.ExceededMemory",
                        (entry->flags & EXCEEDED_MEMORY) == EXCEEDED_MEMORY);
  size_t total_memory = 0, nonshared_memory = 0;
  entry->data->GetMemoryUsage(&total_memory, &nonshared_memory);
  UMA_HISTOGRAM_COUNTS("Storage.Blob.TotalSize", total_memory / 1024);
  UMA_HISTOGRAM_COUNTS("Storage.Blob.TotalUnsharedSize",
                       nonshared_memory / 1024);
  TRACE_COUNTER1("Blob", "MemoryStoreUsageBytes", memory_usage_);
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
    size_t memory_freeing = 0;
    if (entry->IsBeingBuilt()) {
      memory_freeing = entry->data_builder->GetNonsharedMemoryUsage();
      entry->data_builder->RemoveBlobFromShareableItems(uuid);
    } else {
      memory_freeing = entry->data->GetUnsharedMemoryUsage();
      entry->data->RemoveBlobFromShareableItems(uuid);
    }
    DCHECK_LE(memory_freeing, memory_usage_);
    memory_usage_ -= memory_freeing;
    delete entry;
    blob_map_.erase(found);
  }
}

void BlobStorageContext::BlobEntryExceededMemory(BlobMapEntry* entry) {
  // If we're using too much memory, drop this blob's data.
  // TODO(michaeln): Blob memory storage does not yet spill over to disk,
  // as a stop gap, we'll prevent memory usage over a max amount.
  memory_usage_ -= entry->data_builder->GetNonsharedMemoryUsage();
  entry->flags |= EXCEEDED_MEMORY;
  entry->data_builder.reset(new InternalBlobData::Builder());
}

scoped_refptr<BlobDataItem> BlobStorageContext::AllocateBlobItem(
    const std::string& uuid,
    const DataElement& ipc_data) {
  scoped_refptr<BlobDataItem> blob_item;

  uint64_t length = ipc_data.length();
  scoped_ptr<DataElement> element(new DataElement());
  switch (ipc_data.type()) {
    case DataElement::TYPE_BYTES:
      DCHECK(!ipc_data.offset());
      element->SetToBytes(ipc_data.bytes(), length);
      blob_item = new BlobDataItem(std::move(element));
      break;
    case DataElement::TYPE_FILE:
      element->SetToFilePathRange(ipc_data.path(), ipc_data.offset(), length,
                                  ipc_data.expected_modification_time());
      blob_item = new BlobDataItem(
          std::move(element), ShareableFileReference::Get(ipc_data.path()));
      break;
    case DataElement::TYPE_FILE_FILESYSTEM:
      element->SetToFileSystemUrlRange(ipc_data.filesystem_url(),
                                       ipc_data.offset(), length,
                                       ipc_data.expected_modification_time());
      blob_item = new BlobDataItem(std::move(element));
      break;
    case DataElement::TYPE_BLOB:
      // This is a temporary item that will be deconstructed later.
      element->SetToBlobRange(ipc_data.blob_uuid(), ipc_data.offset(),
                              ipc_data.length());
      blob_item = new BlobDataItem(std::move(element));
      break;
    case DataElement::TYPE_DISK_CACHE_ENTRY:  // This type can't be sent by IPC.
      NOTREACHED();
      break;
    default:
      NOTREACHED();
      break;
  }

  return blob_item;
}

bool BlobStorageContext::AppendAllocatedBlobItem(
    const std::string& target_blob_uuid,
    scoped_refptr<BlobDataItem> blob_item,
    InternalBlobData::Builder* target_blob_builder) {
  bool exceeded_memory = false;

  // The blob data is stored in the canonical way which only contains a
  // list of Data, File, and FileSystem items. Aggregated TYPE_BLOB items
  // are expanded into the primitive constituent types and reused if possible.
  // 1) The Data item is denoted by the raw data and length.
  // 2) The File item is denoted by the file path, the range and the expected
  //    modification time.
  // 3) The FileSystem File item is denoted by the FileSystem URL, the range
  //    and the expected modification time.
  // 4) The Blob item is denoted by the source blob and an offset and size.
  //    Internal items that are fully used by the new blob (not cut by the
  //    offset or size) are shared between the blobs.  Otherwise, the relevant
  //    portion of the item is copied.

  const DataElement& data_element = blob_item->data_element();
  uint64_t length = data_element.length();
  uint64_t offset = data_element.offset();
  UMA_HISTOGRAM_COUNTS("Storage.Blob.StorageSizeBeforeAppend",
                       memory_usage_ / 1024);
  switch (data_element.type()) {
    case DataElement::TYPE_BYTES:
      UMA_HISTOGRAM_COUNTS("Storage.BlobItemSize.Bytes", length / 1024);
      DCHECK(!offset);
      if (memory_usage_ + length > kMaxMemoryUsage) {
        exceeded_memory = true;
        break;
      }
      memory_usage_ += length;
      target_blob_builder->AppendSharedBlobItem(
          new ShareableBlobDataItem(target_blob_uuid, blob_item));
      break;
    case DataElement::TYPE_FILE: {
      bool full_file = (length == std::numeric_limits<uint64_t>::max());
      UMA_HISTOGRAM_BOOLEAN("Storage.BlobItemSize.File.Unknown", full_file);
      if (!full_file) {
        UMA_HISTOGRAM_COUNTS("Storage.BlobItemSize.File",
                             (length - offset) / 1024);
      }
      target_blob_builder->AppendSharedBlobItem(
          new ShareableBlobDataItem(target_blob_uuid, blob_item));
      break;
    }
    case DataElement::TYPE_FILE_FILESYSTEM: {
      bool full_file = (length == std::numeric_limits<uint64_t>::max());
      UMA_HISTOGRAM_BOOLEAN("Storage.BlobItemSize.FileSystem.Unknown",
                            full_file);
      if (!full_file) {
        UMA_HISTOGRAM_COUNTS("Storage.BlobItemSize.FileSystem",
                             (length - offset) / 1024);
      }
      target_blob_builder->AppendSharedBlobItem(
          new ShareableBlobDataItem(target_blob_uuid, blob_item));
      break;
    }
    case DataElement::TYPE_BLOB: {
      UMA_HISTOGRAM_COUNTS("Storage.BlobItemSize.Blob",
                           (length - offset) / 1024);
      // We grab the handle to ensure it stays around while we copy it.
      scoped_ptr<BlobDataHandle> src =
          GetBlobDataFromUUID(data_element.blob_uuid());
      if (src) {
        BlobMapEntry* other_entry =
            blob_map_.find(data_element.blob_uuid())->second;
        DCHECK(other_entry->data);
        exceeded_memory = !AppendBlob(target_blob_uuid, *other_entry->data,
                                      offset, length, target_blob_builder);
      }
      break;
    }
    case DataElement::TYPE_DISK_CACHE_ENTRY: {
      UMA_HISTOGRAM_COUNTS("Storage.BlobItemSize.CacheEntry",
                           (length - offset) / 1024);
      target_blob_builder->AppendSharedBlobItem(
          new ShareableBlobDataItem(target_blob_uuid, blob_item));
      break;
    }
    default:
      NOTREACHED();
      break;
  }
  UMA_HISTOGRAM_COUNTS("Storage.Blob.StorageSizeAfterAppend",
                       memory_usage_ / 1024);

  return !exceeded_memory;
}

bool BlobStorageContext::AppendBlob(
    const std::string& target_blob_uuid,
    const InternalBlobData& blob,
    uint64_t offset,
    uint64_t length,
    InternalBlobData::Builder* target_blob_builder) {
  DCHECK(length > 0);

  const std::vector<scoped_refptr<ShareableBlobDataItem>>& items = blob.items();
  auto iter = items.begin();
  if (offset) {
    for (; iter != items.end(); ++iter) {
      const BlobDataItem& item = *(iter->get()->item());
      if (offset >= item.length())
        offset -= item.length();
      else
        break;
    }
  }

  for (; iter != items.end() && length > 0; ++iter) {
    scoped_refptr<ShareableBlobDataItem> shareable_item = iter->get();
    const BlobDataItem& item = *(shareable_item->item());
    uint64_t item_length = item.length();
    DCHECK_GT(item_length, offset);
    uint64_t current_length = item_length - offset;
    uint64_t new_length = current_length > length ? length : current_length;

    bool reusing_blob_item = offset == 0 && new_length == item.length();
    UMA_HISTOGRAM_BOOLEAN("Storage.Blob.ReusedItem", reusing_blob_item);
    if (reusing_blob_item) {
      shareable_item->referencing_blobs().insert(target_blob_uuid);
      target_blob_builder->AppendSharedBlobItem(shareable_item);
      length -= new_length;
      continue;
    }

    // We need to do copying of the items when we have a different offset or
    // length
    switch (item.type()) {
      case DataElement::TYPE_BYTES: {
        UMA_HISTOGRAM_COUNTS("Storage.BlobItemSize.BlobSlice.Bytes",
                             new_length / 1024);
        if (memory_usage_ + new_length > kMaxMemoryUsage) {
          return false;
        }
        DCHECK(!item.offset());
        scoped_ptr<DataElement> element(new DataElement());
        element->SetToBytes(item.bytes() + offset,
                            static_cast<int64_t>(new_length));
        memory_usage_ += new_length;
        target_blob_builder->AppendSharedBlobItem(new ShareableBlobDataItem(
            target_blob_uuid, new BlobDataItem(std::move(element))));
      } break;
      case DataElement::TYPE_FILE: {
        DCHECK_NE(item.length(), std::numeric_limits<uint64_t>::max())
            << "We cannot use a section of a file with an unknown length";
        UMA_HISTOGRAM_COUNTS("Storage.BlobItemSize.BlobSlice.File",
                             new_length / 1024);
        scoped_ptr<DataElement> element(new DataElement());
        element->SetToFilePathRange(item.path(), item.offset() + offset,
                                    new_length,
                                    item.expected_modification_time());
        target_blob_builder->AppendSharedBlobItem(new ShareableBlobDataItem(
            target_blob_uuid,
            new BlobDataItem(std::move(element), item.data_handle_)));
      } break;
      case DataElement::TYPE_FILE_FILESYSTEM: {
        UMA_HISTOGRAM_COUNTS("Storage.BlobItemSize.BlobSlice.FileSystem",
                             new_length / 1024);
        scoped_ptr<DataElement> element(new DataElement());
        element->SetToFileSystemUrlRange(item.filesystem_url(),
                                         item.offset() + offset, new_length,
                                         item.expected_modification_time());
        target_blob_builder->AppendSharedBlobItem(new ShareableBlobDataItem(
            target_blob_uuid, new BlobDataItem(std::move(element))));
      } break;
      case DataElement::TYPE_DISK_CACHE_ENTRY: {
        scoped_ptr<DataElement> element(new DataElement());
        element->SetToDiskCacheEntryRange(item.offset() + offset,
                                          new_length);
        target_blob_builder->AppendSharedBlobItem(new ShareableBlobDataItem(
            target_blob_uuid,
            new BlobDataItem(std::move(element), item.data_handle_,
                             item.disk_cache_entry(),
                             item.disk_cache_stream_index())));
      } break;
      default:
        CHECK(false) << "Illegal blob item type: " << item.type();
    }
    length -= new_length;
    offset = 0;
  }
  return true;
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
