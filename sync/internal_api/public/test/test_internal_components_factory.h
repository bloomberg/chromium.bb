// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_TEST_TEST_INTERNAL_COMPONENTS_FACTORY_H_
#define SYNC_INTERNAL_API_PUBLIC_TEST_TEST_INTERNAL_COMPONENTS_FACTORY_H_

#include "sync/internal_api/public/internal_components_factory.h"

namespace syncer {

class TestInternalComponentsFactory : public InternalComponentsFactory {
 public:
  explicit TestInternalComponentsFactory(const Switches& switches,
                                         StorageOption option,
                                         StorageOption* storage_used);
  virtual ~TestInternalComponentsFactory();

  virtual scoped_ptr<SyncScheduler> BuildScheduler(
      const std::string& name,
      sessions::SyncSessionContext* context,
      syncer::CancelationSignal* cancelation_signal) OVERRIDE;

  virtual scoped_ptr<sessions::SyncSessionContext> BuildContext(
      ServerConnectionManager* connection_manager,
      syncable::Directory* directory,
      ExtensionsActivity* monitor,
      const std::vector<SyncEngineEventListener*>& listeners,
      sessions::DebugInfoGetter* debug_info_getter,
      ModelTypeRegistry* model_type_registry,
      const std::string& invalidator_client_id) OVERRIDE;

  virtual scoped_ptr<syncable::DirectoryBackingStore>
  BuildDirectoryBackingStore(
      StorageOption storage,
      const std::string& dir_name,
      const base::FilePath& backing_filepath) OVERRIDE;

  virtual Switches GetSwitches() const OVERRIDE;

 private:
  const Switches switches_;
  const StorageOption storage_override_;
  StorageOption* storage_used_;

  DISALLOW_COPY_AND_ASSIGN(TestInternalComponentsFactory);
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_TEST_TEST_INTERNAL_COMPONENTS_FACTORY_H_
