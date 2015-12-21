// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_STORAGE_REGISTRY_H_
#define STORAGE_BROWSER_BLOB_BLOB_STORAGE_REGISTRY_H_

#include <stddef.h>

#include <map>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/macros.h"
#include "storage/browser/blob/internal_blob_data.h"
#include "storage/browser/storage_browser_export.h"

class GURL;

namespace storage {

// This class stores the blob data in the various states of construction, as
// well as URL mappings to blob uuids.
// Implementation notes:
// * There is no implicit refcounting in this class, except for setting the
//   refcount to 1 on registration.
// * When removing a uuid registration, we do not check for URL mappings to that
//   uuid. The user must keep track of these.
class STORAGE_EXPORT BlobStorageRegistry {
 public:
  enum class BlobState {
    // First the renderer reserves the uuid.
    RESERVED = 1,
    // Second, we are asynchronously transporting data to the browser.
    ASYNC_TRANSPORTATION,
    // Third, we construct the blob when we have all of the data.
    CONSTRUCTION,
    // Finally, the blob is built.
    ACTIVE
  };

  struct STORAGE_EXPORT Entry {
    size_t refcount;
    BlobState state;
    std::vector<base::Callback<void(bool)>> construction_complete_callbacks;

    // Flags
    bool exceeded_memory;

    // data and data_builder are mutually exclusive.
    scoped_ptr<InternalBlobData> data;
    scoped_ptr<InternalBlobData::Builder> data_builder;

    Entry() = delete;
    Entry(int refcount, BlobState state);
    ~Entry();

    // Performs a test-and-set on the state of the given blob. If the state
    // isn't as expected, we return false. Otherwise we set the new state and
    // return true.
    bool TestAndSetState(BlobState expected, BlobState set);
  };

  BlobStorageRegistry();
  ~BlobStorageRegistry();

  // Creates the blob entry with a refcount of 1 and a state of RESERVED. If
  // the blob is already in use, we return null.
  Entry* CreateEntry(const std::string& uuid);

  // Removes the blob entry with the given uuid. This does not unmap any
  // URLs that are pointing to this uuid. Returns if the entry existed.
  bool DeleteEntry(const std::string& uuid);

  // Gets the blob entry for the given uuid. Returns nullptr if the entry
  // does not exist.
  Entry* GetEntry(const std::string& uuid);

  // Creates a url mapping from blob uuid to the given url. Returns false if
  // the uuid isn't mapped to an entry or if there already is a map for the URL.
  bool CreateUrlMapping(const GURL& url, const std::string& uuid);

  // Removes the given URL mapping. Optionally populates a uuid string of the
  // removed entry uuid. Returns false if the url isn't mapped.
  bool DeleteURLMapping(const GURL& url, std::string* uuid);

  // Returns if the url is mapped to a blob uuid.
  bool IsURLMapped(const GURL& blob_url) const;

  // Returns the entry from the given url, and optionally populates the uuid for
  // that entry. Returns a nullptr if the mapping or entry doesn't exist.
  Entry* GetEntryFromURL(const GURL& url, std::string* uuid);

  size_t blob_count() const { return blob_map_.size(); }
  size_t url_count() const { return url_to_uuid_.size(); }

 private:
  using BlobMap = base::ScopedPtrHashMap<std::string, scoped_ptr<Entry>>;
  using URLMap = std::map<GURL, std::string>;

  BlobMap blob_map_;
  URLMap url_to_uuid_;

  DISALLOW_COPY_AND_ASSIGN(BlobStorageRegistry);
};

}  // namespace storage
#endif  // STORAGE_BROWSER_BLOB_BLOB_STORAGE_REGISTRY_H_
