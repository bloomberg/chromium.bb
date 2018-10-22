// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/directory_data_type_controller.h"

#include <utility>
#include <vector>

#include "base/threading/thread_task_runner_handle.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/engine/model_type_configurer.h"
#include "components/sync/syncable/syncable_read_transaction.h"
#include "components/sync/syncable/user_share.h"

namespace syncer {

DirectoryDataTypeController::DirectoryDataTypeController(
    ModelType type,
    const base::Closure& dump_stack,
    SyncClient* sync_client,
    ModelSafeGroup model_safe_group)
    : DataTypeController(type),
      dump_stack_(dump_stack),
      sync_client_(sync_client),
      model_safe_group_(model_safe_group) {}

DirectoryDataTypeController::~DirectoryDataTypeController() {}

bool DirectoryDataTypeController::ShouldLoadModelBeforeConfigure() const {
  // Directory datatypes don't require loading models before configure. Their
  // progress markers are stored in directory and can be extracted without
  // datatype participation.
  return false;
}

void DirectoryDataTypeController::BeforeLoadModels(
    ModelTypeConfigurer* configurer) {
  configurer->RegisterDirectoryDataType(type(), model_safe_group_);
}

void DirectoryDataTypeController::RegisterWithBackend(
    base::Callback<void(bool)> set_downloaded,
    ModelTypeConfigurer* configurer) {}

void DirectoryDataTypeController::ActivateDataType(
    ModelTypeConfigurer* configurer) {
  DCHECK(CalledOnValidThread());
  // Tell the backend about the change processor for this type so it can
  // begin routing changes to it.
  configurer->ActivateDirectoryDataType(type(), model_safe_group_,
                                        GetChangeProcessor());
}

void DirectoryDataTypeController::DeactivateDataType(
    ModelTypeConfigurer* configurer) {
  DCHECK(CalledOnValidThread());
  configurer->DeactivateDirectoryDataType(type());
  configurer->UnregisterDirectoryDataType(type());
}

void DirectoryDataTypeController::Stop(SyncStopMetadataFate metadata_fate,
                                       StopCallback callback) {
  DCHECK(CalledOnValidThread());
  Stop(metadata_fate);
  std::move(callback).Run();
}

void DirectoryDataTypeController::GetAllNodes(
    const AllNodesCallback& callback) {
  std::unique_ptr<base::ListValue> node_list = GetAllNodesForTypeFromDirectory(
      type(), sync_client_->GetSyncService()->GetUserShare()->directory.get());
  callback.Run(type(), std::move(node_list));
}

void DirectoryDataTypeController::GetStatusCounters(
    const StatusCountersCallback& callback) {
  std::vector<int> num_entries_by_type(syncer::MODEL_TYPE_COUNT, 0);
  std::vector<int> num_to_delete_entries_by_type(syncer::MODEL_TYPE_COUNT, 0);
  sync_client_->GetSyncService()
      ->GetUserShare()
      ->directory->CollectMetaHandleCounts(&num_entries_by_type,
                                           &num_to_delete_entries_by_type);
  syncer::StatusCounters counters;
  counters.num_entries_and_tombstones = num_entries_by_type[type()];
  counters.num_entries =
      num_entries_by_type[type()] - num_to_delete_entries_by_type[type()];

  callback.Run(type(), counters);
}

void DirectoryDataTypeController::RecordMemoryUsageAndCountsHistograms() {
  syncer::syncable::Directory* directory =
      sync_client_->GetSyncService()->GetUserShare()->directory.get();
  SyncRecordModelTypeMemoryHistogram(
      type(), directory->EstimateMemoryUsageByType(type()));
  SyncRecordModelTypeCountHistogram(type(),
                                    directory->CountEntriesByType(type()));
}

// static
std::unique_ptr<base::ListValue>
DirectoryDataTypeController::GetAllNodesForTypeFromDirectory(
    ModelType type,
    syncable::Directory* directory) {
  DCHECK(directory);
  syncable::ReadTransaction trans(FROM_HERE, directory);
  return directory->GetNodeDetailsForType(&trans, type);
}

}  // namespace syncer
