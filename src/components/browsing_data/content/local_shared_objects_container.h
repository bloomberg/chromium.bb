// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_LOCAL_SHARED_OBJECTS_CONTAINER_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_LOCAL_SHARED_OBJECTS_CONTAINER_H_

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/browsing_data/content/cookie_helper.h"
#include "storage/common/file_system/file_system_types.h"

class GURL;

namespace content {
class BrowserContext;
}

namespace browsing_data {
class CannedAppCacheHelper;
class CannedCacheStorageHelper;
class CannedCookieHelper;
class CannedDatabaseHelper;
class CannedFileSystemHelper;
class CannedIndexedDBHelper;
class CannedServiceWorkerHelper;
class CannedSharedWorkerHelper;
class CannedLocalStorageHelper;

class LocalSharedObjectsContainer {
 public:
  explicit LocalSharedObjectsContainer(
      content::BrowserContext* browser_context,
      const std::vector<storage::FileSystemType>& additional_file_system_types,
      browsing_data::CookieHelper::IsDeletionDisabledCallback callback);
  ~LocalSharedObjectsContainer();

  // Returns the number of objects stored in the container.
  size_t GetObjectCount() const;

  // Returns the number of objects for the given |origin|.
  size_t GetObjectCountForDomain(const GURL& origin) const;

  // Get number of unique registrable domains in the container.
  size_t GetDomainCount() const;

  // Empties the container.
  void Reset();

  CannedAppCacheHelper* appcaches() const { return appcaches_.get(); }
  CannedCookieHelper* cookies() const { return cookies_.get(); }
  CannedDatabaseHelper* databases() const { return databases_.get(); }
  CannedFileSystemHelper* file_systems() const { return file_systems_.get(); }
  CannedIndexedDBHelper* indexed_dbs() const { return indexed_dbs_.get(); }
  CannedLocalStorageHelper* local_storages() const {
    return local_storages_.get();
  }
  CannedServiceWorkerHelper* service_workers() const {
    return service_workers_.get();
  }
  CannedSharedWorkerHelper* shared_workers() const {
    return shared_workers_.get();
  }
  CannedCacheStorageHelper* cache_storages() const {
    return cache_storages_.get();
  }
  CannedLocalStorageHelper* session_storages() const {
    return session_storages_.get();
  }

 private:
  scoped_refptr<CannedAppCacheHelper> appcaches_;
  scoped_refptr<CannedCookieHelper> cookies_;
  scoped_refptr<CannedDatabaseHelper> databases_;
  scoped_refptr<CannedFileSystemHelper> file_systems_;
  scoped_refptr<CannedIndexedDBHelper> indexed_dbs_;
  scoped_refptr<CannedLocalStorageHelper> local_storages_;
  scoped_refptr<CannedServiceWorkerHelper> service_workers_;
  scoped_refptr<CannedSharedWorkerHelper> shared_workers_;
  scoped_refptr<CannedCacheStorageHelper> cache_storages_;
  scoped_refptr<CannedLocalStorageHelper> session_storages_;

  DISALLOW_COPY_AND_ASSIGN(LocalSharedObjectsContainer);
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_LOCAL_SHARED_OBJECTS_CONTAINER_H_
