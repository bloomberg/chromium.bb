// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SYSTEM_STORAGE_STORAGE_INFO_PROVIDER_H_
#define EXTENSIONS_BROWSER_API_SYSTEM_STORAGE_STORAGE_INFO_PROVIDER_H_

#include <set>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "extensions/browser/api/system_info/system_info_provider.h"
#include "extensions/common/api/system_storage.h"

namespace storage_monitor {
class StorageInfo;
}

namespace extensions {

namespace systeminfo {

// Build StorageUnitInfo struct from StorageInfo instance. The |unit|
// parameter is the output value.
void BuildStorageUnitInfo(const storage_monitor::StorageInfo& info,
                          api::system_storage::StorageUnitInfo* unit);

}  // namespace systeminfo

typedef std::vector<api::system_storage::StorageUnitInfo> StorageUnitInfoList;

class StorageInfoProvider : public SystemInfoProvider {
 public:
  typedef base::Callback<void(const std::string&, double)>
      GetStorageFreeSpaceCallback;

  // Get the single shared instance of StorageInfoProvider.
  static StorageInfoProvider* Get();

  // SystemInfoProvider implementations
  void PrepareQueryOnUIThread() override;
  void InitializeProvider(const base::Closure& do_query_info_callback) override;

  virtual double GetStorageFreeSpaceFromTransientIdAsync(
      const std::string& transient_id);

  const StorageUnitInfoList& storage_unit_info_list() const { return info_; }

  static void InitializeForTesting(scoped_refptr<StorageInfoProvider> provider);

 protected:
  StorageInfoProvider();

  ~StorageInfoProvider() override;

  // Put all available storages' information into |info_|.
  void GetAllStoragesIntoInfoList();

  // The last information filled up by QueryInfo and is accessed on multiple
  // threads, but the whole class is being guarded by SystemInfoProvider base
  // class.
  //
  // |info_| is accessed on the UI thread while |is_waiting_for_completion_| is
  // false and on the sequenced worker pool while |is_waiting_for_completion_|
  // is true.
  StorageUnitInfoList info_;

 private:
  // SystemInfoProvider implementations.
  // Override to query the available capacity of all known storage devices on
  // the blocking pool, including fixed and removable devices.
  bool QueryInfo() override;

  static base::LazyInstance<
      scoped_refptr<StorageInfoProvider>>::DestructorAtExit provider_;

  DISALLOW_COPY_AND_ASSIGN(StorageInfoProvider);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SYSTEM_STORAGE_STORAGE_INFO_PROVIDER_H_
