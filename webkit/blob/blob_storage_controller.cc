// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/blob/blob_storage_controller.h"

#include "base/logging.h"
#include "googleurl/src/gurl.h"
#include "webkit/blob/blob_data.h"

namespace webkit_blob {

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

static const int64 kMaxMemoryUsage = 1024 * 1024 * 1024;  // 1G

}  // namespace

BlobStorageController::BlobStorageController()
    : memory_usage_(0) {
}

BlobStorageController::~BlobStorageController() {
}

void BlobStorageController::StartBuildingBlob(const GURL& url) {
  DCHECK(url.SchemeIs("blob"));
  DCHECK(!BlobUrlHasRef(url));
  BlobData* blob_data = new BlobData;
  unfinalized_blob_map_[url.spec()] = blob_data;
  IncrementBlobDataUsage(blob_data);
}

void BlobStorageController::AppendBlobDataItem(
    const GURL& url, const BlobData::Item& item) {
  DCHECK(url.SchemeIs("blob"));
  DCHECK(!BlobUrlHasRef(url));
  BlobMap::iterator found = unfinalized_blob_map_.find(url.spec());
  if (found == unfinalized_blob_map_.end())
    return;
  BlobData* target_blob_data = found->second;
  DCHECK(target_blob_data);

  memory_usage_ -= target_blob_data->GetMemoryUsage();

  // The blob data is stored in the "canonical" way. That is, it only contains a
  // list of Data and File items.
  // 1) The Data item is denoted by the raw data and the range.
  // 2) The File item is denoted by the file path, the range and the expected
  //    modification time.
  // 3) The FileSystem File item is denoted by the FileSystem URL, the range
  //    and the expected modification time.
  // All the Blob items in the passing blob data are resolved and expanded into
  // a set of Data and File items.

  DCHECK(item.length() > 0);
  switch (item.type()) {
    case BlobData::Item::TYPE_BYTES:
      DCHECK(!item.offset());
      target_blob_data->AppendData(item.bytes(), item.length());
      break;
    case BlobData::Item::TYPE_FILE:
      AppendFileItem(target_blob_data,
                     item.path(),
                     item.offset(),
                     item.length(),
                     item.expected_modification_time());
      break;
    case BlobData::Item::TYPE_FILE_FILESYSTEM:
      AppendFileSystemFileItem(target_blob_data,
                               item.url(),
                               item.offset(),
                               item.length(),
                               item.expected_modification_time());
      break;
    case BlobData::Item::TYPE_BLOB: {
      BlobData* src_blob_data = GetBlobDataFromUrl(item.url());
      DCHECK(src_blob_data);
      if (src_blob_data)
        AppendStorageItems(target_blob_data,
                           src_blob_data,
                           item.offset(),
                           item.length());
      break;
    }
    default:
      NOTREACHED();
      break;
  }

  memory_usage_ += target_blob_data->GetMemoryUsage();

  // If we're using too much memory, drop this blob.
  // TODO(michaeln): Blob memory storage does not yet spill over to disk,
  // until it does, we'll prevent memory usage over a max amount.
  if (memory_usage_ > kMaxMemoryUsage)
    RemoveBlob(url);
}

void BlobStorageController::FinishBuildingBlob(
    const GURL& url, const std::string& content_type) {
  DCHECK(url.SchemeIs("blob"));
  DCHECK(!BlobUrlHasRef(url));
  BlobMap::iterator found = unfinalized_blob_map_.find(url.spec());
  if (found == unfinalized_blob_map_.end())
    return;
  found->second->set_content_type(content_type);
  blob_map_[url.spec()] = found->second;
  unfinalized_blob_map_.erase(found);
}

void BlobStorageController::AddFinishedBlob(const GURL& url,
                                            const BlobData* data) {
  StartBuildingBlob(url);
  for (std::vector<BlobData::Item>::const_iterator iter =
           data->items().begin();
       iter != data->items().end(); ++iter) {
    AppendBlobDataItem(url, *iter);
  }
  FinishBuildingBlob(url, data->content_type());
}

void BlobStorageController::CloneBlob(
    const GURL& url, const GURL& src_url) {
  DCHECK(url.SchemeIs("blob"));
  DCHECK(!BlobUrlHasRef(url));

  BlobData* blob_data = GetBlobDataFromUrl(src_url);
  DCHECK(blob_data);
  if (!blob_data)
    return;

  blob_map_[url.spec()] = blob_data;
  IncrementBlobDataUsage(blob_data);
}

void BlobStorageController::RemoveBlob(const GURL& url) {
  DCHECK(url.SchemeIs("blob"));
  DCHECK(!BlobUrlHasRef(url));

  if (!RemoveFromMapHelper(&unfinalized_blob_map_, url))
    RemoveFromMapHelper(&blob_map_, url);
}

bool BlobStorageController::RemoveFromMapHelper(
    BlobMap* map, const GURL& url) {
  BlobMap::iterator found = map->find(url.spec());
  if (found == map->end())
    return false;
  if (DecrementBlobDataUsage(found->second))
    memory_usage_ -= found->second->GetMemoryUsage();
  map->erase(found);
  return true;
}


BlobData* BlobStorageController::GetBlobDataFromUrl(const GURL& url) {
  BlobMap::iterator found = blob_map_.find(
      BlobUrlHasRef(url) ? ClearBlobUrlRef(url).spec() : url.spec());
  return (found != blob_map_.end()) ? found->second : NULL;
}

void BlobStorageController::AppendStorageItems(
    BlobData* target_blob_data, BlobData* src_blob_data,
    uint64 offset, uint64 length) {
  DCHECK(target_blob_data && src_blob_data &&
         length != static_cast<uint64>(-1));

  std::vector<BlobData::Item>::const_iterator iter =
      src_blob_data->items().begin();
  if (offset) {
    for (; iter != src_blob_data->items().end(); ++iter) {
      if (offset >= iter->length())
        offset -= iter->length();
      else
        break;
    }
  }

  for (; iter != src_blob_data->items().end() && length > 0; ++iter) {
    uint64 current_length = iter->length() - offset;
    uint64 new_length = current_length > length ? length : current_length;
    if (iter->type() == BlobData::Item::TYPE_BYTES) {
      target_blob_data->AppendData(
          iter->bytes() + static_cast<size_t>(iter->offset() + offset),
          static_cast<uint32>(new_length));
    } else if (iter->type() == BlobData::Item::TYPE_FILE) {
      AppendFileItem(target_blob_data,
                     iter->path(),
                     iter->offset() + offset,
                     new_length,
                     iter->expected_modification_time());
    } else {
      DCHECK(iter->type() == BlobData::Item::TYPE_FILE_FILESYSTEM);
      AppendFileSystemFileItem(target_blob_data,
                               iter->url(),
                               iter->offset() + offset,
                               new_length,
                               iter->expected_modification_time());
    }
    length -= new_length;
    offset = 0;
  }
}

void BlobStorageController::AppendFileItem(
    BlobData* target_blob_data,
    const base::FilePath& file_path, uint64 offset, uint64 length,
    const base::Time& expected_modification_time) {
  target_blob_data->AppendFile(file_path, offset, length,
                               expected_modification_time);

  // It may be a temporary file that should be deleted when no longer needed.
  scoped_refptr<ShareableFileReference> shareable_file =
      ShareableFileReference::Get(file_path);
  if (shareable_file)
    target_blob_data->AttachShareableFileReference(shareable_file);
}

void BlobStorageController::AppendFileSystemFileItem(
    BlobData* target_blob_data,
    const GURL& url, uint64 offset, uint64 length,
    const base::Time& expected_modification_time) {
  target_blob_data->AppendFileSystemFile(url, offset, length,
                                         expected_modification_time);
}

void BlobStorageController::IncrementBlobDataUsage(BlobData* blob_data) {
  blob_data_usage_count_[blob_data] += 1;
}

bool BlobStorageController::DecrementBlobDataUsage(BlobData* blob_data) {
  BlobDataUsageMap::iterator found = blob_data_usage_count_.find(blob_data);
  DCHECK(found != blob_data_usage_count_.end());
  if (--(found->second))
    return false;  // Still in use
  blob_data_usage_count_.erase(found);
  return true;
}

}  // namespace webkit_blob
