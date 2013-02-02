// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BLOB_BLOB_STORAGE_CONTROLLER_H_
#define WEBKIT_BLOB_BLOB_STORAGE_CONTROLLER_H_

#include <map>
#include <string>

#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/process.h"
#include "webkit/blob/blob_data.h"
#include "webkit/storage/webkit_storage_export.h"

class GURL;

namespace base {
class FilePath;
class Time;
}

namespace webkit_blob {

// This class handles the logistics of blob Storage within the browser process.
class WEBKIT_STORAGE_EXPORT BlobStorageController {
 public:
  BlobStorageController();
  ~BlobStorageController();

  void StartBuildingBlob(const GURL& url);
  void AppendBlobDataItem(const GURL& url, const BlobData::Item& data_item);
  void FinishBuildingBlob(const GURL& url, const std::string& content_type);
  void AddFinishedBlob(const GURL& url, const BlobData* blob_data);
  void CloneBlob(const GURL& url, const GURL& src_url);
  void RemoveBlob(const GURL& url);
  BlobData* GetBlobDataFromUrl(const GURL& url);

 private:
  friend class ViewBlobInternalsJob;

  typedef base::hash_map<std::string, scoped_refptr<BlobData> > BlobMap;
  typedef std::map<BlobData*, int> BlobDataUsageMap;

  void AppendStorageItems(BlobData* target_blob_data,
                          BlobData* src_blob_data,
                          uint64 offset,
                          uint64 length);
  void AppendFileItem(BlobData* target_blob_data,
                      const base::FilePath& file_path, uint64 offset,
                      uint64 length,
                      const base::Time& expected_modification_time);
  void AppendFileSystemFileItem(
      BlobData* target_blob_data,
      const GURL& url, uint64 offset, uint64 length,
      const base::Time& expected_modification_time);

  bool RemoveFromMapHelper(BlobMap* map, const GURL& url);

  void IncrementBlobDataUsage(BlobData* blob_data);
  // Returns true if no longer in use.
  bool DecrementBlobDataUsage(BlobData* blob_data);

  BlobMap blob_map_;
  BlobMap unfinalized_blob_map_;

  // Used to keep track of how much memory is being utitlized for blob data,
  // we count only the items of TYPE_DATA which are held in memory and not
  // items of TYPE_FILE.
  int64 memory_usage_;

  // Multiple urls can refer to the same blob data, this map keeps track of
  // how many urls refer to a BlobData.
  BlobDataUsageMap blob_data_usage_count_;

  DISALLOW_COPY_AND_ASSIGN(BlobStorageController);
};

}  // namespace webkit_blob

#endif  // WEBKIT_BLOB_BLOB_STORAGE_CONTROLLER_H_
