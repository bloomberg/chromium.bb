// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_STORAGE_CONTEXT_H_
#define STORAGE_BROWSER_BLOB_BLOB_STORAGE_CONTEXT_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_data_item.h"
#include "storage/browser/blob/blob_data_snapshot.h"
#include "storage/browser/blob/internal_blob_data.h"
#include "storage/browser/blob/shareable_blob_data_item.h"
#include "storage/browser/storage_browser_export.h"
#include "storage/common/data_element.h"

class GURL;

namespace base {
class FilePath;
class Time;
}

namespace content {
class BlobStorageHost;
}

namespace storage {

class BlobDataBuilder;
class InternalBlobData;

// This class handles the logistics of blob Storage within the browser process,
// and maintains a mapping from blob uuid to the data. The class is single
// threaded and should only be used on the IO thread.
// In chromium, there is one instance per profile.
class STORAGE_EXPORT BlobStorageContext
    : public base::SupportsWeakPtr<BlobStorageContext> {
 public:
  BlobStorageContext();
  ~BlobStorageContext();

  scoped_ptr<BlobDataHandle> GetBlobDataFromUUID(const std::string& uuid);
  scoped_ptr<BlobDataHandle> GetBlobDataFromPublicURL(const GURL& url);

  // Useful for coining blobs from within the browser process. If the
  // blob cannot be added due to memory consumption, returns NULL.
  // A builder should not be used twice to create blobs, as the internal
  // resources are refcounted instead of copied for memory efficiency.
  // To cleanly use a builder multiple times, please call Clone() on the
  // builder, or even better for memory savings, clear the builder and append
  // the previously constructed blob.
  scoped_ptr<BlobDataHandle> AddFinishedBlob(const BlobDataBuilder& builder);

  // Deprecated, use const ref version above.
  scoped_ptr<BlobDataHandle> AddFinishedBlob(const BlobDataBuilder* builder);

  // Useful for coining blob urls from within the browser process.
  bool RegisterPublicBlobURL(const GURL& url, const std::string& uuid);
  void RevokePublicBlobURL(const GURL& url);

  size_t memory_usage() const { return memory_usage_; }
  size_t blob_count() const { return blob_map_.size(); }

 private:
  friend class content::BlobStorageHost;
  friend class BlobDataHandle::BlobDataHandleShared;
  friend class ViewBlobInternalsJob;

  enum EntryFlags {
    EXCEEDED_MEMORY = 1 << 1,
  };

  struct BlobMapEntry {
    int refcount;
    int flags;
    // data and data_builder are mutually exclusive.
    scoped_ptr<InternalBlobData> data;
    scoped_ptr<InternalBlobData::Builder> data_builder;

    BlobMapEntry();
    BlobMapEntry(int refcount, InternalBlobData::Builder* data);
    ~BlobMapEntry();

    bool IsBeingBuilt();
  };

  typedef std::map<std::string, BlobMapEntry*> BlobMap;
  typedef std::map<GURL, std::string> BlobURLMap;

  // Called by BlobDataHandle.
  scoped_ptr<BlobDataSnapshot> CreateSnapshot(const std::string& uuid);

  // ### Methods called by BlobStorageHost ###
  void StartBuildingBlob(const std::string& uuid);
  void AppendBlobDataItem(const std::string& uuid,
                          const DataElement& data_item);
  void FinishBuildingBlob(const std::string& uuid, const std::string& type);
  void CancelBuildingBlob(const std::string& uuid);
  void IncrementBlobRefCount(const std::string& uuid);
  void DecrementBlobRefCount(const std::string& uuid);
  // #########################################

  // Flags the entry for exceeding memory, and resets the builder.
  void BlobEntryExceededMemory(BlobMapEntry* entry);

  // Allocates memory to hold the given data element and copies the data over.
  scoped_refptr<BlobDataItem> AllocateBlobItem(const std::string& uuid,
                                               const DataElement& data_item);

  // Appends the given blob item to the blob builder.  The new blob
  // retains ownership of data_item if applicable, and returns false if there
  // wasn't enough memory to hold the item.
  bool AppendAllocatedBlobItem(const std::string& target_blob_uuid,
                               scoped_refptr<BlobDataItem> data_item,
                               InternalBlobData::Builder* target_blob_data);

  // Deconstructs the blob and appends it's contents to the target blob.  Items
  // are shared if possible, and copied if the given offset and length
  // have to split an item.
  bool AppendBlob(const std::string& target_blob_uuid,
                  const InternalBlobData& blob,
                  uint64_t offset,
                  uint64_t length,
                  InternalBlobData::Builder* target_blob_data);

  bool IsInUse(const std::string& uuid);
  bool IsBeingBuilt(const std::string& uuid);
  bool IsUrlRegistered(const GURL& blob_url);

  BlobMap blob_map_;
  BlobURLMap public_blob_urls_;

  // Used to keep track of how much memory is being utilized for blob data,
  // we count only the items of TYPE_DATA which are held in memory and not
  // items of TYPE_FILE.
  size_t memory_usage_;

  DISALLOW_COPY_AND_ASSIGN(BlobStorageContext);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_BLOB_BLOB_STORAGE_CONTEXT_H_
