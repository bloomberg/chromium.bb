// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_STORAGE_CONTEXT_H_
#define STORAGE_BROWSER_BLOB_BLOB_STORAGE_CONTEXT_H_

#include <map>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_data_snapshot.h"
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

class BlobDataHandle;
class BlobDataBuilder;

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
  scoped_ptr<BlobDataHandle> AddFinishedBlob(const BlobDataBuilder& blob_data);

  // Useful for coining blob urls from within the browser process.
  bool RegisterPublicBlobURL(const GURL& url, const std::string& uuid);
  void RevokePublicBlobURL(const GURL& url);

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
    scoped_ptr<BlobDataSnapshot> data;
    scoped_ptr<BlobDataBuilder> data_builder;

    BlobMapEntry();
    BlobMapEntry(int refcount, BlobDataBuilder* data);
    ~BlobMapEntry();

    bool IsBeingBuilt();
  };

  typedef std::map<std::string, BlobMapEntry*> BlobMap;
  typedef std::map<GURL, std::string> BlobURLMap;

  // Called by BlobDataHandle.
  scoped_ptr<BlobDataSnapshot> CreateSnapshot(const std::string& uuid);

  void StartBuildingBlob(const std::string& uuid);
  void AppendBlobDataItem(const std::string& uuid,
                          const DataElement& data_item);
  void FinishBuildingBlob(const std::string& uuid, const std::string& type);
  void CancelBuildingBlob(const std::string& uuid);
  void IncrementBlobRefCount(const std::string& uuid);
  void DecrementBlobRefCount(const std::string& uuid);

  bool ExpandStorageItems(BlobDataBuilder* target_blob_data,
                          const BlobDataSnapshot& src_blob_data,
                          uint64 offset,
                          uint64 length);
  bool AppendBytesItem(BlobDataBuilder* target_blob_data,
                       const char* data,
                       int64 length);
  void AppendFileItem(BlobDataBuilder* target_blob_data,
                      const base::FilePath& file_path,
                      uint64 offset,
                      uint64 length,
                      const base::Time& expected_modification_time);
  void AppendFileSystemFileItem(BlobDataBuilder* target_blob_data,
                                const GURL& url,
                                uint64 offset,
                                uint64 length,
                                const base::Time& expected_modification_time);

  bool IsInUse(const std::string& uuid);
  bool IsBeingBuilt(const std::string& uuid);
  bool IsUrlRegistered(const GURL& blob_url);

  BlobMap blob_map_;
  BlobURLMap public_blob_urls_;

  // Used to keep track of how much memory is being utilized for blob data,
  // we count only the items of TYPE_DATA which are held in memory and not
  // items of TYPE_FILE.
  int64 memory_usage_;

  DISALLOW_COPY_AND_ASSIGN(BlobStorageContext);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_BLOB_BLOB_STORAGE_CONTEXT_H_
