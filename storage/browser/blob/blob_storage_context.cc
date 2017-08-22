// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_storage_context.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_item.h"
#include "storage/browser/blob/blob_data_snapshot.h"
#include "storage/browser/blob/shareable_blob_data_item.h"
#include "storage/common/data_element.h"
#include "url/gurl.h"

namespace storage {
namespace {
using ItemCopyEntry = BlobEntry::ItemCopyEntry;
using QuotaAllocationTask = BlobMemoryController::QuotaAllocationTask;

bool IsBytes(DataElement::Type type) {
  return type == DataElement::TYPE_BYTES ||
         type == DataElement::TYPE_BYTES_DESCRIPTION;
}

void RecordBlobItemSizeStats(const DataElement& input_element) {
  uint64_t length = input_element.length();

  switch (input_element.type()) {
    case DataElement::TYPE_BYTES:
    case DataElement::TYPE_BYTES_DESCRIPTION:
      UMA_HISTOGRAM_COUNTS_1M("Storage.BlobItemSize.Bytes", length / 1024);
      break;
    case DataElement::TYPE_BLOB:
      UMA_HISTOGRAM_COUNTS_1M("Storage.BlobItemSize.Blob",
                              (length - input_element.offset()) / 1024);
      break;
    case DataElement::TYPE_FILE: {
      bool full_file = (length == std::numeric_limits<uint64_t>::max());
      UMA_HISTOGRAM_BOOLEAN("Storage.BlobItemSize.File.Unknown", full_file);
      if (!full_file) {
        UMA_HISTOGRAM_COUNTS_1M("Storage.BlobItemSize.File",
                                (length - input_element.offset()) / 1024);
      }
      break;
    }
    case DataElement::TYPE_FILE_FILESYSTEM: {
      bool full_file = (length == std::numeric_limits<uint64_t>::max());
      UMA_HISTOGRAM_BOOLEAN("Storage.BlobItemSize.FileSystem.Unknown",
                            full_file);
      if (!full_file) {
        UMA_HISTOGRAM_COUNTS_1M("Storage.BlobItemSize.FileSystem",
                                (length - input_element.offset()) / 1024);
      }
      break;
    }
    case DataElement::TYPE_DISK_CACHE_ENTRY:
      UMA_HISTOGRAM_COUNTS_1M("Storage.BlobItemSize.CacheEntry",
                              (length - input_element.offset()) / 1024);
      break;
    case DataElement::TYPE_UNKNOWN:
      NOTREACHED();
      break;
  }
}
}  // namespace

BlobStorageContext::BlobFlattener::BlobFlattener(
    const BlobDataBuilder& input_builder,
    BlobEntry* output_blob,
    BlobStorageRegistry* registry) {
  const std::string& uuid = input_builder.uuid_;
  std::set<std::string> dependent_blob_uuids;

  size_t num_files_with_unknown_size = 0;
  size_t num_building_dependent_blobs = 0;

  bool found_memory_transport = false;
  bool found_file_transport = false;

  base::CheckedNumeric<uint64_t> checked_total_size = 0;
  base::CheckedNumeric<uint64_t> checked_total_memory_size = 0;
  base::CheckedNumeric<uint64_t> checked_transport_quota_needed = 0;
  base::CheckedNumeric<uint64_t> checked_copy_quota_needed = 0;

  for (scoped_refptr<BlobDataItem> input_item : input_builder.items_) {
    const DataElement& input_element = input_item->data_element();
    DataElement::Type type = input_element.type();
    uint64_t length = input_element.length();

    RecordBlobItemSizeStats(input_element);

    if (IsBytes(type)) {
      DCHECK_NE(0 + DataElement::kUnknownSize, input_element.length());
      found_memory_transport = true;
      if (found_file_transport) {
        // We cannot have both memory and file transport items.
        status = BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;
        return;
      }
      contains_unpopulated_transport_items |=
          (type == DataElement::TYPE_BYTES_DESCRIPTION);
      checked_transport_quota_needed += length;
      checked_total_size += length;
      scoped_refptr<ShareableBlobDataItem> item = new ShareableBlobDataItem(
          std::move(input_item), ShareableBlobDataItem::QUOTA_NEEDED);
      pending_transport_items.push_back(item);
      transport_items.push_back(item.get());
      output_blob->AppendSharedBlobItem(std::move(item));
      continue;
    }
    if (type == DataElement::TYPE_BLOB) {
      BlobEntry* ref_entry = registry->GetEntry(input_element.blob_uuid());

      if (!ref_entry || input_element.blob_uuid() == uuid) {
        status = BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;
        return;
      }

      if (BlobStatusIsError(ref_entry->status())) {
        status = BlobStatus::ERR_REFERENCED_BLOB_BROKEN;
        return;
      }

      if (ref_entry->total_size() == DataElement::kUnknownSize) {
        // We can't reference a blob with unknown size.
        status = BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;
        return;
      }

      if (dependent_blob_uuids.find(input_element.blob_uuid()) ==
          dependent_blob_uuids.end()) {
        dependent_blobs.push_back(
            std::make_pair(input_element.blob_uuid(), ref_entry));
        dependent_blob_uuids.insert(input_element.blob_uuid());
        if (BlobStatusIsPending(ref_entry->status())) {
          num_building_dependent_blobs++;
        }
      }

      length = length == DataElement::kUnknownSize ? ref_entry->total_size()
                                                   : input_element.length();
      checked_total_size += length;

      // If we're referencing the whole blob, then we don't need to slice.
      if (input_element.offset() == 0 && length == ref_entry->total_size()) {
        for (const auto& shareable_item : ref_entry->items()) {
          output_blob->AppendSharedBlobItem(shareable_item);
        }
        continue;
      }

      // Validate our reference has good offset & length.
      if (input_element.offset() + length > ref_entry->total_size()) {
        status = BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;
        return;
      }

      BlobSlice slice(*ref_entry, input_element.offset(), length);

      if (!slice.copying_memory_size.IsValid() ||
          !slice.total_memory_size.IsValid()) {
        status = BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;
        return;
      }
      checked_total_memory_size += slice.total_memory_size;

      if (slice.first_source_item) {
        copies.push_back(ItemCopyEntry(slice.first_source_item,
                                       slice.first_item_slice_offset,
                                       slice.dest_items.front()));
        pending_copy_items.push_back(slice.dest_items.front());
      }
      if (slice.last_source_item) {
        copies.push_back(
            ItemCopyEntry(slice.last_source_item, 0, slice.dest_items.back()));
        pending_copy_items.push_back(slice.dest_items.back());
      }
      checked_copy_quota_needed += slice.copying_memory_size;

      for (auto& shareable_item : slice.dest_items) {
        output_blob->AppendSharedBlobItem(std::move(shareable_item));
      }
      continue;
    }

    // If the source item is a temporary file item, then we need to keep track
    // of that and mark it as needing quota.
    scoped_refptr<ShareableBlobDataItem> item;
    if (BlobDataBuilder::IsFutureFileItem(input_element)) {
      found_file_transport = true;
      if (found_memory_transport) {
        // We cannot have both memory and file transport items.
        status = BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;
        return;
      }
      contains_unpopulated_transport_items = true;
      item = new ShareableBlobDataItem(std::move(input_item),
                                       ShareableBlobDataItem::QUOTA_NEEDED);
      pending_transport_items.push_back(item);
      transport_items.push_back(item.get());
      checked_transport_quota_needed += length;
    } else {
      item = new ShareableBlobDataItem(
          std::move(input_item),
          ShareableBlobDataItem::POPULATED_WITHOUT_QUOTA);
    }
    if (length == DataElement::kUnknownSize)
      num_files_with_unknown_size++;

    checked_total_size += length;
    output_blob->AppendSharedBlobItem(std::move(item));
  }

  if (num_files_with_unknown_size > 1 && input_builder.items_.size() > 1) {
    status = BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;
    return;
  }
  if (!checked_total_size.IsValid() || !checked_total_memory_size.IsValid() ||
      !checked_transport_quota_needed.IsValid() ||
      !checked_copy_quota_needed.IsValid()) {
    status = BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;
    return;
  }
  total_size = checked_total_size.ValueOrDie();
  total_memory_size = checked_total_memory_size.ValueOrDie();
  transport_quota_needed = checked_transport_quota_needed.ValueOrDie();
  copy_quota_needed = checked_copy_quota_needed.ValueOrDie();
  transport_quota_type = found_file_transport ? TransportQuotaType::FILE
                                              : TransportQuotaType::MEMORY;
  if (transport_quota_needed) {
    status = BlobStatus::PENDING_QUOTA;
  } else {
    status = BlobStatus::PENDING_INTERNALS;
  }
}

BlobStorageContext::BlobFlattener::~BlobFlattener() {}

BlobStorageContext::BlobSlice::BlobSlice(const BlobEntry& source,
                                         uint64_t slice_offset,
                                         uint64_t slice_size) {
  const auto& source_items = source.items();
  const auto& offsets = source.offsets();
  DCHECK_LE(slice_offset + slice_size, source.total_size());
  size_t item_index =
      std::upper_bound(offsets.begin(), offsets.end(), slice_offset) -
      offsets.begin();
  uint64_t item_offset =
      item_index == 0 ? slice_offset : slice_offset - offsets[item_index - 1];
  size_t num_items = source_items.size();

  size_t first_item_index = item_index;

  // Read starting from 'first_item_index' and 'item_offset'.
  for (uint64_t total_sliced = 0;
       item_index < num_items && total_sliced < slice_size; item_index++) {
    const scoped_refptr<BlobDataItem>& source_item =
        source_items[item_index]->item();
    uint64_t source_length = source_item->length();
    DataElement::Type type = source_item->type();
    DCHECK_NE(source_length, std::numeric_limits<uint64_t>::max());
    DCHECK_NE(source_length, 0ull);

    uint64_t read_size =
        std::min(source_length - item_offset, slice_size - total_sliced);
    total_sliced += read_size;

    bool reusing_blob_item = (read_size == source_length);
    UMA_HISTOGRAM_BOOLEAN("Storage.Blob.ReusedItem", reusing_blob_item);
    if (reusing_blob_item) {
      // We can share the entire item.
      dest_items.push_back(source_items[item_index]);
      if (IsBytes(type)) {
        total_memory_size += source_length;
      }
      continue;
    }

    scoped_refptr<BlobDataItem> data_item;
    ShareableBlobDataItem::State state =
        ShareableBlobDataItem::POPULATED_WITHOUT_QUOTA;
    switch (type) {
      case DataElement::TYPE_BYTES_DESCRIPTION:
      case DataElement::TYPE_BYTES: {
        UMA_HISTOGRAM_COUNTS_1M("Storage.BlobItemSize.BlobSlice.Bytes",
                                read_size / 1024);
        if (item_index == first_item_index) {
          first_item_slice_offset = item_offset;
          first_source_item = source_items[item_index];
        } else {
          last_source_item = source_items[item_index];
        }
        copying_memory_size += read_size;
        total_memory_size += read_size;
        // Since we don't have quota yet for memory, we create temporary items
        // for this data. When our blob is finished constructing, all dependent
        // blobs are done, and we have enough memory quota, we'll copy the data
        // over.
        std::unique_ptr<DataElement> element(new DataElement());
        element->SetToBytesDescription(base::checked_cast<size_t>(read_size));
        data_item = new BlobDataItem(std::move(element));
        state = ShareableBlobDataItem::QUOTA_NEEDED;
        break;
      }
      case DataElement::TYPE_FILE: {
        UMA_HISTOGRAM_COUNTS_1M("Storage.BlobItemSize.BlobSlice.File",
                                read_size / 1024);
        std::unique_ptr<DataElement> element(new DataElement());
        element->SetToFilePathRange(
            source_item->path(), source_item->offset() + item_offset, read_size,
            source_item->expected_modification_time());
        data_item =
            new BlobDataItem(std::move(element), source_item->data_handle_);

        if (BlobDataBuilder::IsFutureFileItem(source_item->data_element())) {
          // The source file isn't a real file yet (path is fake), so store the
          // items we need to copy from later.
          if (item_index == first_item_index) {
            first_item_slice_offset = item_offset;
            first_source_item = source_items[item_index];
          } else {
            last_source_item = source_items[item_index];
          }
        }
        break;
      }
      case DataElement::TYPE_FILE_FILESYSTEM: {
        UMA_HISTOGRAM_COUNTS_1M("Storage.BlobItemSize.BlobSlice.FileSystem",
                                read_size / 1024);
        std::unique_ptr<DataElement> element(new DataElement());
        element->SetToFileSystemUrlRange(
            source_item->filesystem_url(), source_item->offset() + item_offset,
            read_size, source_item->expected_modification_time());
        data_item = new BlobDataItem(std::move(element));
        break;
      }
      case DataElement::TYPE_DISK_CACHE_ENTRY: {
        UMA_HISTOGRAM_COUNTS_1M("Storage.BlobItemSize.BlobSlice.CacheEntry",
                                read_size / 1024);
        std::unique_ptr<DataElement> element(new DataElement());
        element->SetToDiskCacheEntryRange(source_item->offset() + item_offset,
                                          read_size);
        data_item =
            new BlobDataItem(std::move(element), source_item->data_handle_,
                             source_item->disk_cache_entry(),
                             source_item->disk_cache_stream_index(),
                             source_item->disk_cache_side_stream_index());
        break;
      }
      case DataElement::TYPE_BLOB:
      case DataElement::TYPE_UNKNOWN:
        CHECK(false) << "Illegal blob item type: " << type;
    }
    dest_items.push_back(
        new ShareableBlobDataItem(std::move(data_item), state));
    item_offset = 0;
  }
}

BlobStorageContext::BlobSlice::~BlobSlice() {}

BlobStorageContext::BlobStorageContext()
    : memory_controller_(base::FilePath(), scoped_refptr<base::TaskRunner>()),
      ptr_factory_(this) {}

BlobStorageContext::BlobStorageContext(
    base::FilePath storage_directory,
    scoped_refptr<base::TaskRunner> file_runner)
    : memory_controller_(std::move(storage_directory), std::move(file_runner)),
      ptr_factory_(this) {}

BlobStorageContext::~BlobStorageContext() {}

std::unique_ptr<BlobDataHandle> BlobStorageContext::GetBlobDataFromUUID(
    const std::string& uuid) {
  BlobEntry* entry = registry_.GetEntry(uuid);
  if (!entry)
    return nullptr;
  return CreateHandle(uuid, entry);
}

std::unique_ptr<BlobDataHandle> BlobStorageContext::GetBlobDataFromPublicURL(
    const GURL& url) {
  std::string uuid;
  BlobEntry* entry = registry_.GetEntryFromURL(url, &uuid);
  if (!entry)
    return nullptr;
  return CreateHandle(uuid, entry);
}

std::unique_ptr<BlobDataHandle> BlobStorageContext::AddFinishedBlob(
    const BlobDataBuilder& external_builder) {
  TRACE_EVENT0("Blob", "Context::AddFinishedBlob");
  return BuildBlob(external_builder, TransportAllowedCallback());
}

std::unique_ptr<BlobDataHandle> BlobStorageContext::AddFinishedBlob(
    const BlobDataBuilder* builder) {
  DCHECK(builder);
  return AddFinishedBlob(*builder);
}

std::unique_ptr<BlobDataHandle> BlobStorageContext::AddBrokenBlob(
    const std::string& uuid,
    const std::string& content_type,
    const std::string& content_disposition,
    BlobStatus reason) {
  DCHECK(!registry_.HasEntry(uuid));
  DCHECK(BlobStatusIsError(reason));
  BlobEntry* entry =
      registry_.CreateEntry(uuid, content_type, content_disposition);
  entry->set_status(reason);
  FinishBuilding(entry);
  return CreateHandle(uuid, entry);
}

bool BlobStorageContext::RegisterPublicBlobURL(const GURL& blob_url,
                                               const std::string& uuid) {
  if (!registry_.CreateUrlMapping(blob_url, uuid))
    return false;
  IncrementBlobRefCount(uuid);
  return true;
}

void BlobStorageContext::RevokePublicBlobURL(const GURL& blob_url) {
  std::string uuid;
  if (!registry_.DeleteURLMapping(blob_url, &uuid))
    return;
  DecrementBlobRefCount(uuid);
}

std::unique_ptr<BlobDataHandle> BlobStorageContext::AddFutureBlob(
    const std::string& uuid,
    const std::string& content_type,
    const std::string& content_disposition) {
  DCHECK(!registry_.HasEntry(uuid));

  BlobEntry* entry =
      registry_.CreateEntry(uuid, content_type, content_disposition);
  entry->set_size(DataElement::kUnknownSize);
  entry->set_status(BlobStatus::PENDING_CONSTRUCTION);
  entry->set_building_state(base::MakeUnique<BlobEntry::BuildingState>(
      false, TransportAllowedCallback(), 0));
  return CreateHandle(uuid, entry);
}

std::unique_ptr<BlobDataHandle> BlobStorageContext::BuildPreregisteredBlob(
    const BlobDataBuilder& content,
    const TransportAllowedCallback& transport_allowed_callback) {
  BlobEntry* entry = registry_.GetEntry(content.uuid());
  DCHECK(entry);
  DCHECK_EQ(BlobStatus::PENDING_CONSTRUCTION, entry->status());
  entry->set_size(0);

  return BuildBlobInternal(entry, content, transport_allowed_callback);
}

std::unique_ptr<BlobDataHandle> BlobStorageContext::BuildBlob(
    const BlobDataBuilder& content,
    const TransportAllowedCallback& transport_allowed_callback) {
  DCHECK(!registry_.HasEntry(content.uuid_));

  BlobEntry* entry = registry_.CreateEntry(
      content.uuid(), content.content_type_, content.content_disposition_);

  return BuildBlobInternal(entry, content, transport_allowed_callback);
}

std::unique_ptr<BlobDataHandle> BlobStorageContext::BuildBlobInternal(
    BlobEntry* entry,
    const BlobDataBuilder& content,
    const TransportAllowedCallback& transport_allowed_callback) {
  // This flattens all blob references in the transportion content out and
  // stores the complete item representation in the internal data.
  BlobFlattener flattener(content, entry, &registry_);

  DCHECK(!flattener.contains_unpopulated_transport_items ||
         transport_allowed_callback)
      << "If we have pending unpopulated content then a callback is required";

  DCHECK(flattener.total_size == 0 ||
         flattener.total_size == entry->total_size());
  entry->set_size(flattener.total_size);
  entry->set_status(flattener.status);
  std::unique_ptr<BlobDataHandle> handle = CreateHandle(content.uuid_, entry);

  UMA_HISTOGRAM_COUNTS_1M("Storage.Blob.ItemCount", entry->items().size());
  UMA_HISTOGRAM_COUNTS_1M("Storage.Blob.TotalSize",
                          flattener.total_memory_size / 1024);

  uint64_t total_memory_needed =
      flattener.copy_quota_needed +
      (flattener.transport_quota_type == TransportQuotaType::MEMORY
           ? flattener.transport_quota_needed
           : 0);
  UMA_HISTOGRAM_COUNTS_1M("Storage.Blob.TotalUnsharedSize",
                          total_memory_needed / 1024);

  size_t num_building_dependent_blobs = 0;
  std::vector<std::unique_ptr<BlobDataHandle>> dependent_blobs;
  // We hold a handle to all blobs we're using. This is important, as our memory
  // accounting can be delayed until OnEnoughSizeForBlobData is called, and we
  // only free memory on canceling when we've done this accounting. If a
  // dependent blob is dereferenced, then we're the last blob holding onto that
  // data item, and we need to account for that. So we prevent that case by
  // holding onto all blobs.
  for (const std::pair<std::string, BlobEntry*>& pending_blob :
       flattener.dependent_blobs) {
    dependent_blobs.push_back(
        CreateHandle(pending_blob.first, pending_blob.second));
    if (BlobStatusIsPending(pending_blob.second->status())) {
      pending_blob.second->building_state_->build_completion_callbacks
          .push_back(base::Bind(&BlobStorageContext::OnDependentBlobFinished,
                                ptr_factory_.GetWeakPtr(), content.uuid_));
      num_building_dependent_blobs++;
    }
  }

  auto previous_building_state = std::move(entry->building_state_);
  entry->set_building_state(base::MakeUnique<BlobEntry::BuildingState>(
      !flattener.pending_transport_items.empty(), transport_allowed_callback,
      num_building_dependent_blobs));
  BlobEntry::BuildingState* building_state = entry->building_state_.get();
  std::swap(building_state->copies, flattener.copies);
  std::swap(building_state->dependent_blobs, dependent_blobs);
  std::swap(building_state->transport_items, flattener.transport_items);
  if (previous_building_state) {
    DCHECK(!previous_building_state->transport_items_present);
    DCHECK(!previous_building_state->transport_allowed_callback);
    DCHECK(previous_building_state->transport_items.empty());
    DCHECK(previous_building_state->dependent_blobs.empty());
    DCHECK(previous_building_state->copies.empty());
    std::swap(building_state->build_completion_callbacks,
              previous_building_state->build_completion_callbacks);
    auto runner = base::ThreadTaskRunnerHandle::Get();
    for (const auto& callback :
         previous_building_state->build_started_callbacks)
      runner->PostTask(FROM_HERE, base::BindOnce(callback, entry->status()));
  }

  // Break ourselves if we have an error. BuildingState must be set first so the
  // callback is called correctly.
  if (BlobStatusIsError(flattener.status)) {
    CancelBuildingBlobInternal(entry, flattener.status);
    return handle;
  }

  // Avoid the state where we might grant only one quota.
  if (!memory_controller_.CanReserveQuota(flattener.copy_quota_needed +
                                          flattener.transport_quota_needed)) {
    CancelBuildingBlobInternal(entry, BlobStatus::ERR_OUT_OF_MEMORY);
    return handle;
  }

  if (flattener.copy_quota_needed > 0) {
    // The blob can complete during the execution of |ReserveMemoryQuota|.
    base::WeakPtr<QuotaAllocationTask> pending_request =
        memory_controller_.ReserveMemoryQuota(
            std::move(flattener.pending_copy_items),
            base::Bind(&BlobStorageContext::OnEnoughSpaceForCopies,
                       ptr_factory_.GetWeakPtr(), content.uuid_));
    // Building state will be null if the blob is already finished.
    if (entry->building_state_)
      entry->building_state_->copy_quota_request = std::move(pending_request);
  }

  if (flattener.transport_quota_needed > 0) {
    base::WeakPtr<QuotaAllocationTask> pending_request;

    switch (flattener.transport_quota_type) {
      case TransportQuotaType::MEMORY: {
        // The blob can complete during the execution of |ReserveMemoryQuota|.
        std::vector<BlobMemoryController::FileCreationInfo> empty_files;
        pending_request = memory_controller_.ReserveMemoryQuota(
            std::move(flattener.pending_transport_items),
            base::Bind(&BlobStorageContext::OnEnoughSpaceForTransport,
                       ptr_factory_.GetWeakPtr(), content.uuid_,
                       base::Passed(&empty_files)));
        break;
      }
      case TransportQuotaType::FILE:
        pending_request = memory_controller_.ReserveFileQuota(
            std::move(flattener.pending_transport_items),
            base::Bind(&BlobStorageContext::OnEnoughSpaceForTransport,
                       ptr_factory_.GetWeakPtr(), content.uuid_));
        break;
    }

    // Building state will be null if the blob is already finished.
    if (entry->building_state_) {
      entry->building_state_->transport_quota_request =
          std::move(pending_request);
    }
  }

  if (entry->CanFinishBuilding())
    FinishBuilding(entry);

  return handle;
}

void BlobStorageContext::CancelBuildingBlob(const std::string& uuid,
                                            BlobStatus reason) {
  CancelBuildingBlobInternal(registry_.GetEntry(uuid), reason);
}

void BlobStorageContext::NotifyTransportComplete(const std::string& uuid) {
  BlobEntry* entry = registry_.GetEntry(uuid);
  CHECK(entry) << "There is no blob entry with uuid " << uuid;
  DCHECK(BlobStatusIsPending(entry->status()));
  NotifyTransportCompleteInternal(entry);
}

void BlobStorageContext::IncrementBlobRefCount(const std::string& uuid) {
  BlobEntry* entry = registry_.GetEntry(uuid);
  DCHECK(entry);
  entry->IncrementRefCount();
}

void BlobStorageContext::DecrementBlobRefCount(const std::string& uuid) {
  BlobEntry* entry = registry_.GetEntry(uuid);
  DCHECK(entry);
  DCHECK_GT(entry->refcount(), 0u);
  entry->DecrementRefCount();
  if (entry->refcount() == 0) {
    DVLOG(1) << "BlobStorageContext::DecrementBlobRefCount(" << uuid
             << "): Deleting blob.";
    ClearAndFreeMemory(entry);
    registry_.DeleteEntry(uuid);
  }
}

std::unique_ptr<BlobDataSnapshot> BlobStorageContext::CreateSnapshot(
    const std::string& uuid) {
  std::unique_ptr<BlobDataSnapshot> result;
  BlobEntry* entry = registry_.GetEntry(uuid);
  if (entry->status() != BlobStatus::DONE)
    return result;

  std::unique_ptr<BlobDataSnapshot> snapshot(new BlobDataSnapshot(
      uuid, entry->content_type(), entry->content_disposition()));
  snapshot->items_.reserve(entry->items().size());
  for (const auto& shareable_item : entry->items()) {
    snapshot->items_.push_back(shareable_item->item());
  }
  memory_controller_.NotifyMemoryItemsUsed(entry->items());
  return snapshot;
}

BlobStatus BlobStorageContext::GetBlobStatus(const std::string& uuid) const {
  const BlobEntry* entry = registry_.GetEntry(uuid);
  if (!entry)
    return BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;
  return entry->status();
}

void BlobStorageContext::RunOnConstructionComplete(
    const std::string& uuid,
    const BlobStatusCallback& done) {
  BlobEntry* entry = registry_.GetEntry(uuid);
  DCHECK(entry);
  if (BlobStatusIsPending(entry->status())) {
    entry->building_state_->build_completion_callbacks.push_back(done);
    return;
  }
  done.Run(entry->status());
}

void BlobStorageContext::RunOnConstructionBegin(
    const std::string& uuid,
    const BlobStatusCallback& done) {
  BlobEntry* entry = registry_.GetEntry(uuid);
  DCHECK(entry);
  if (entry->status() == BlobStatus::PENDING_CONSTRUCTION) {
    entry->building_state_->build_started_callbacks.push_back(done);
    return;
  }
  done.Run(entry->status());
}

std::unique_ptr<BlobDataHandle> BlobStorageContext::CreateHandle(
    const std::string& uuid,
    BlobEntry* entry) {
  return base::WrapUnique(new BlobDataHandle(
      uuid, entry->content_type_, entry->content_disposition_, entry->size_,
      this, base::ThreadTaskRunnerHandle::Get().get()));
}

void BlobStorageContext::NotifyTransportCompleteInternal(BlobEntry* entry) {
  DCHECK(entry);
  for (ShareableBlobDataItem* shareable_item :
       entry->building_state_->transport_items) {
    DCHECK(shareable_item->state() == ShareableBlobDataItem::QUOTA_GRANTED);
    shareable_item->set_state(ShareableBlobDataItem::POPULATED_WITH_QUOTA);
  }
  entry->set_status(BlobStatus::PENDING_INTERNALS);
  if (entry->CanFinishBuilding())
    FinishBuilding(entry);
}

void BlobStorageContext::CancelBuildingBlobInternal(BlobEntry* entry,
                                                    BlobStatus reason) {
  DCHECK(entry);
  DCHECK(BlobStatusIsError(reason));
  TransportAllowedCallback transport_allowed_callback;
  if (entry->building_state_ &&
      entry->building_state_->transport_allowed_callback) {
    transport_allowed_callback =
        entry->building_state_->transport_allowed_callback;
    entry->building_state_->transport_allowed_callback.Reset();
  }
  if (entry->building_state_ &&
      entry->status() == BlobStatus::PENDING_CONSTRUCTION) {
    auto runner = base::ThreadTaskRunnerHandle::Get();
    for (const auto& callback : entry->building_state_->build_started_callbacks)
      runner->PostTask(FROM_HERE, base::BindOnce(callback, reason));
  }
  ClearAndFreeMemory(entry);
  entry->set_status(reason);
  if (transport_allowed_callback) {
    transport_allowed_callback.Run(
        reason, std::vector<BlobMemoryController::FileCreationInfo>());
  }
  FinishBuilding(entry);
}

void BlobStorageContext::FinishBuilding(BlobEntry* entry) {
  DCHECK(entry);

  BlobStatus status = entry->status_;
  DCHECK_NE(BlobStatus::DONE, status);

  bool error = BlobStatusIsError(status);
  UMA_HISTOGRAM_BOOLEAN("Storage.Blob.Broken", error);
  if (error) {
    UMA_HISTOGRAM_ENUMERATION("Storage.Blob.BrokenReason",
                              static_cast<int>(status),
                              (static_cast<int>(BlobStatus::LAST_ERROR) + 1));
  }

  if (BlobStatusIsPending(entry->status_)) {
    for (const ItemCopyEntry& copy : entry->building_state_->copies) {
      // Our source item can be a file if it was a slice of an unpopulated file,
      // or a slice of data that was then paged to disk.
      size_t dest_size = static_cast<size_t>(copy.dest_item->item()->length());
      DataElement::Type dest_type = copy.dest_item->item()->type();
      switch (copy.source_item->item()->type()) {
        case DataElement::TYPE_BYTES: {
          DCHECK_EQ(dest_type, DataElement::TYPE_BYTES_DESCRIPTION);
          const char* src_data =
              copy.source_item->item()->bytes() + copy.source_item_offset;
          copy.dest_item->item()->item_->SetToBytes(src_data, dest_size);
          break;
        }
        case DataElement::TYPE_FILE: {
          // If we expected a memory item (and our source was paged to disk) we
          // free that memory.
          if (dest_type == DataElement::TYPE_BYTES_DESCRIPTION)
            copy.dest_item->set_memory_allocation(nullptr);

          const DataElement& source_element =
              copy.source_item->item()->data_element();
          std::unique_ptr<DataElement> new_element(new DataElement());
          new_element->SetToFilePathRange(
              source_element.path(),
              source_element.offset() + copy.source_item_offset, dest_size,
              source_element.expected_modification_time());
          scoped_refptr<BlobDataItem> new_item(new BlobDataItem(
              std::move(new_element), copy.source_item->item()->data_handle_));
          copy.dest_item->set_item(std::move(new_item));
          break;
        }
        case DataElement::TYPE_UNKNOWN:
        case DataElement::TYPE_BLOB:
        case DataElement::TYPE_BYTES_DESCRIPTION:
        case DataElement::TYPE_FILE_FILESYSTEM:
        case DataElement::TYPE_DISK_CACHE_ENTRY:
          NOTREACHED();
          break;
      }
      copy.dest_item->set_state(ShareableBlobDataItem::POPULATED_WITH_QUOTA);
    }

    entry->set_status(BlobStatus::DONE);
  }

  std::vector<BlobStatusCallback> callbacks;
  if (entry->building_state_.get()) {
    std::swap(callbacks, entry->building_state_->build_completion_callbacks);
    entry->building_state_.reset();
  }

  memory_controller_.NotifyMemoryItemsUsed(entry->items());

  auto runner = base::ThreadTaskRunnerHandle::Get();
  for (const auto& callback : callbacks)
    runner->PostTask(FROM_HERE, base::Bind(callback, entry->status()));

  for (const auto& shareable_item : entry->items()) {
    DCHECK_NE(DataElement::TYPE_BYTES_DESCRIPTION,
              shareable_item->item()->type());
    DCHECK(shareable_item->IsPopulated()) << shareable_item->state();
  }
}

void BlobStorageContext::RequestTransport(
    BlobEntry* entry,
    std::vector<BlobMemoryController::FileCreationInfo> files) {
  BlobEntry::BuildingState* building_state = entry->building_state_.get();
  if (building_state->transport_allowed_callback) {
    base::ResetAndReturn(&building_state->transport_allowed_callback)
        .Run(BlobStatus::PENDING_TRANSPORT, std::move(files));
    return;
  }
  DCHECK(files.empty());
  NotifyTransportCompleteInternal(entry);
}

void BlobStorageContext::OnEnoughSpaceForTransport(
    const std::string& uuid,
    std::vector<BlobMemoryController::FileCreationInfo> files,
    bool success) {
  if (!success) {
    CancelBuildingBlob(uuid, BlobStatus::ERR_OUT_OF_MEMORY);
    return;
  }
  BlobEntry* entry = registry_.GetEntry(uuid);
  if (!entry || !entry->building_state_.get())
    return;
  BlobEntry::BuildingState& building_state = *entry->building_state_;
  DCHECK(!building_state.transport_quota_request);
  DCHECK(building_state.transport_items_present);

  entry->set_status(BlobStatus::PENDING_TRANSPORT);
  RequestTransport(entry, std::move(files));

  if (entry->CanFinishBuilding())
    FinishBuilding(entry);
}

void BlobStorageContext::OnEnoughSpaceForCopies(const std::string& uuid,
                                                bool success) {
  if (!success) {
    CancelBuildingBlob(uuid, BlobStatus::ERR_OUT_OF_MEMORY);
    return;
  }
  BlobEntry* entry = registry_.GetEntry(uuid);
  if (!entry)
    return;

  if (entry->CanFinishBuilding())
    FinishBuilding(entry);
}

void BlobStorageContext::OnDependentBlobFinished(
    const std::string& owning_blob_uuid,
    BlobStatus status) {
  BlobEntry* entry = registry_.GetEntry(owning_blob_uuid);
  if (!entry || !entry->building_state_)
    return;

  if (BlobStatusIsError(status)) {
    DCHECK_NE(BlobStatus::ERR_BLOB_DEREFERENCED_WHILE_BUILDING, status)
        << "Referenced blob should never be dereferenced while we "
        << "are depending on it, as our system holds a handle.";
    CancelBuildingBlobInternal(entry, BlobStatus::ERR_REFERENCED_BLOB_BROKEN);
    return;
  }
  DCHECK_GT(entry->building_state_->num_building_dependent_blobs, 0u);
  --entry->building_state_->num_building_dependent_blobs;

  if (entry->CanFinishBuilding())
    FinishBuilding(entry);
}

void BlobStorageContext::ClearAndFreeMemory(BlobEntry* entry) {
  if (entry->building_state_)
    entry->building_state_->CancelRequests();
  entry->ClearItems();
  entry->ClearOffsets();
  entry->set_size(0);
}

}  // namespace storage
