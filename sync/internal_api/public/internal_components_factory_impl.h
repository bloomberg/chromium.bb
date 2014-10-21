// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// An InternalComponentsFactory implementation designed for real production /
// normal use.

#ifndef SYNC_INTERNAL_API_PUBLIC_INTERNAL_COMPONENTS_FACTORY_IMPL_H_
#define SYNC_INTERNAL_API_PUBLIC_INTERNAL_COMPONENTS_FACTORY_IMPL_H_

#include "sync/base/sync_export.h"
#include "sync/internal_api/public/internal_components_factory.h"

namespace syncer {

class SYNC_EXPORT InternalComponentsFactoryImpl
    : public InternalComponentsFactory {
 public:
  InternalComponentsFactoryImpl(const Switches& switches);
  ~InternalComponentsFactoryImpl() override;

  scoped_ptr<SyncScheduler> BuildScheduler(
      const std::string& name,
      sessions::SyncSessionContext* context,
      syncer::CancelationSignal* cancelation_signal) override;

  scoped_ptr<sessions::SyncSessionContext> BuildContext(
      ServerConnectionManager* connection_manager,
      syncable::Directory* directory,
      ExtensionsActivity* extensions_activity,
      const std::vector<SyncEngineEventListener*>& listeners,
      sessions::DebugInfoGetter* debug_info_getter,
      ModelTypeRegistry* model_type_registry,
      const std::string& invalidator_client_id) override;

  scoped_ptr<syncable::DirectoryBackingStore> BuildDirectoryBackingStore(
      StorageOption storage,
      const std::string& dir_name,
      const base::FilePath& backing_filepath) override;

  Switches GetSwitches() const override;

 private:
  const Switches switches_;
  DISALLOW_COPY_AND_ASSIGN(InternalComponentsFactoryImpl);
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_INTERNAL_COMPONENTS_FACTORY_IMPL_H_
