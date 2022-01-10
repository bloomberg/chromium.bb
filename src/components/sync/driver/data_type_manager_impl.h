// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_DATA_TYPE_MANAGER_IMPL_H__
#define COMPONENTS_SYNC_DRIVER_DATA_TYPE_MANAGER_IMPL_H__

#include "base/memory/raw_ptr.h"
#include "components/sync/driver/data_type_manager.h"

#include <map>

#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/sync/base/weak_handle.h"
#include "components/sync/driver/configure_context.h"
#include "components/sync/driver/model_load_manager.h"
#include "components/sync/engine/model_type_configurer.h"

namespace syncer {

class DataTypeController;
class DataTypeDebugInfoListener;
class DataTypeEncryptionHandler;
class DataTypeManagerObserver;
struct DataTypeConfigurationStats;

class DataTypeManagerImpl : public DataTypeManager,
                            public ModelLoadManagerDelegate {
 public:
  DataTypeManagerImpl(
      const WeakHandle<DataTypeDebugInfoListener>& debug_info_listener,
      const DataTypeController::TypeMap* controllers,
      const DataTypeEncryptionHandler* encryption_handler,
      ModelTypeConfigurer* configurer,
      DataTypeManagerObserver* observer);

  DataTypeManagerImpl(const DataTypeManagerImpl&) = delete;
  DataTypeManagerImpl& operator=(const DataTypeManagerImpl&) = delete;

  ~DataTypeManagerImpl() override;

  // DataTypeManager interface.
  void Configure(ModelTypeSet desired_types,
                 const ConfigureContext& context) override;
  void DataTypePreconditionChanged(ModelType type) override;
  void ResetDataTypeErrors() override;

  // Needed only for backend migration.
  void PurgeForMigration(ModelTypeSet undesired_types) override;

  void Stop(ShutdownReason reason) override;
  ModelTypeSet GetActiveDataTypes() const override;
  ModelTypeSet GetPurgedDataTypes() const override;
  State state() const override;

  // |ModelLoadManagerDelegate| implementation.
  void OnAllDataTypesReadyForConfigure() override;
  void OnSingleDataTypeWillStop(ModelType type,
                                const SyncError& error) override;

  bool needs_reconfigure_for_test() const { return needs_reconfigure_; }
  ConfigureReason last_configure_reason_for_test() {
    return last_requested_context_.reason;
  }

 private:
  enum DataTypeConfigState {
    CONFIGURE_ACTIVE,    // Actively being configured. Data of such types
                         // will be downloaded if not present locally.
    CONFIGURE_INACTIVE,  // Already configured or to be configured in future.
                         // Data of such types is left as it is, no
                         // downloading or purging.
    DISABLED,            // Not syncing. Disabled by user.
    FATAL,               // Not syncing due to unrecoverable error.
    CRYPTO,              // Not syncing due to a cryptographer error.
    UNREADY,             // Not syncing due to transient error.
  };
  using DataTypeConfigStateMap = std::map<ModelType, DataTypeConfigState>;

  struct AssociationTypesInfo {
    AssociationTypesInfo();
    AssociationTypesInfo(const AssociationTypesInfo& other);
    ~AssociationTypesInfo();

    // Pending types. This is generally the same as
    // |configuration_types_queue_.front()|.
    ModelTypeSet types;
    // Types that have just been downloaded. This includes types that had
    // previously encountered an error and had to be purged.
    // This is a subset of |types|.
    ModelTypeSet first_sync_types;
    // Time at which |types| began downloading.
    base::Time download_start_time;
    // Time at which |types| finished downloading.
    base::Time download_ready_time;
    // The set of types that are higher priority, and were therefore blocking
    // the download of |types|.
    ModelTypeSet higher_priority_types_before;
  };

  // Return model types in |state_map| that match |state|.
  static ModelTypeSet GetDataTypesInState(
      DataTypeConfigState state,
      const DataTypeConfigStateMap& state_map);

  // Set state of |types| in |state_map| to |state|.
  static void SetDataTypesState(DataTypeConfigState state,
                                ModelTypeSet types,
                                DataTypeConfigStateMap* state_map);

  // Prepare the parameters for the configurer's configuration.
  ModelTypeConfigurer::ConfigureParams PrepareConfigureParams(
      const AssociationTypesInfo& association_types_info);

  // Divide |types| into sets by their priorities and return the sets from
  // high priority to low priority.
  base::queue<ModelTypeSet> PrioritizeTypes(const ModelTypeSet& types);

  // Update precondition state of types in data_type_status_table_ to match
  // value of DataTypeController::GetPreconditionState().
  void UpdatePreconditionErrors(const ModelTypeSet& desired_types);

  // Update precondition state for |type|, such that data_type_status_table_
  // matches DataTypeController::GetPreconditionState(). Returns true if there
  // was an actual change.
  bool UpdatePreconditionError(ModelType type);

  // Starts a reconfiguration if it's required and no configuration is running.
  void ProcessReconfigure();

  // Programmatically force reconfiguration of all data types (if needed).
  void ForceReconfiguration();

  void Restart();

  void NotifyStart();
  void NotifyDone(const ConfigureResult& result);

  void ConfigureImpl(ModelTypeSet desired_types,
                     const ConfigureContext& context);

  // Calls data type controllers of requested types to connect.
  void ConnectDataTypes();

  DataTypeConfigStateMap BuildDataTypeConfigStateMap(
      const ModelTypeSet& types_being_configured) const;

  // Start configuration of next set of types in |configuration_types_queue_|
  // (if any exist, does nothing otherwise).
  void StartNextConfiguration(ModelTypeSet higher_priority_types_before);
  void ConfigurationCompleted(AssociationTypesInfo association_types_info,
                              ModelTypeSet configured_types,
                              ModelTypeSet succeeded_configuration_types,
                              ModelTypeSet failed_configuration_types);

  void RecordConfigurationStats(
      const AssociationTypesInfo& association_types_info);
  void RecordConfigurationStatsImpl(
      const AssociationTypesInfo& association_types_info,
      ModelType type,
      ModelTypeSet same_priority_types_configured_before);

  void StopImpl(ShutdownReason reason);

  ModelTypeSet GetEnabledTypes() const;

  const raw_ptr<ModelTypeConfigurer> configurer_;

  // Map of all data type controllers that are available for sync.
  // This list is determined at startup by various command line flags.
  const raw_ptr<const DataTypeController::TypeMap> controllers_;

  State state_ = DataTypeManager::STOPPED;

  // The set of types whose initial download of sync data has completed.
  // Note: This class mostly doesn't handle control types (i.e. NIGORI) -
  // |controllers_| doesn't contain an entry for NIGORI, and by the time this
  // class gets instantiated, NIGORI is already up and running. It still has to
  // be maintained as part of |downloaded_types_|, however, since in some edge
  // cases (notably PurgeForMigration()), this class might have to trigger a
  // re-download of NIGORI data.
  ModelTypeSet downloaded_types_ = ControlTypes();

  // Types that requested in current configuration cycle.
  ModelTypeSet last_requested_types_;

  // Context information (e.g. the reason) for the last reconfigure attempt.
  ConfigureContext last_requested_context_;

  // A set of types that were enabled at the time of Restart().
  ModelTypeSet last_enabled_types_;

  // A set of types that should be redownloaded even if initial sync is
  // completed for them.
  ModelTypeSet force_redownload_types_;

  // Whether an attempt to reconfigure was made while we were busy configuring.
  // The |last_requested_types_| will reflect the newest set of requested types.
  bool needs_reconfigure_ = false;

  // The last time Restart() was called.
  base::Time last_restart_time_;

  // Sync's datatype debug info listener, which we pass model configuration
  // statistics to.
  const WeakHandle<DataTypeDebugInfoListener> debug_info_listener_;

  // The manager that loads the local models of the data types.
  ModelLoadManager model_load_manager_;

  // DataTypeManager must have only one observer -- the SyncServiceImpl that
  // created it and manages its lifetime.
  const raw_ptr<DataTypeManagerObserver> observer_;

  // For querying failed data types (having unrecoverable error) when
  // configuring backend.
  DataTypeStatusTable data_type_status_table_;

  // Types waiting to be configured, prioritized (highest priority first).
  base::queue<ModelTypeSet> configuration_types_queue_;

  // The encryption handler lets the DataTypeManager know the state of sync
  // datatype encryption.
  raw_ptr<const DataTypeEncryptionHandler> encryption_handler_;

  // Timing stats of data type configuration.
  std::map<ModelType, DataTypeConfigurationStats> configuration_stats_;

  base::WeakPtrFactory<DataTypeManagerImpl> weak_ptr_factory_{this};
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_DATA_TYPE_MANAGER_IMPL_H__
