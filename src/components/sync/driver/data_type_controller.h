// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_DATA_TYPE_CONTROLLER_H__
#define COMPONENTS_SYNC_DRIVER_DATA_TYPE_CONTROLLER_H__

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/sync_stop_metadata_fate.h"
#include "components/sync/base/unrecoverable_error_handler.h"
#include "components/sync/engine/cycle/status_counters.h"
#include "components/sync/model/data_type_error_handler.h"

namespace syncer {

struct ConfigureContext;
class ModelTypeConfigurer;
class SyncError;
class SyncMergeResult;

// DataTypeControllers are responsible for managing the state of a single data
// type. They are not thread safe and should only be used on the UI thread.
class DataTypeController : public base::SupportsWeakPtr<DataTypeController> {
 public:
  enum State {
    NOT_RUNNING,     // The controller has never been started or has previously
                     // been stopped.  Must be in this state to start.
    MODEL_STARTING,  // The controller is waiting on dependent services
                     // that need to be available before model
                     // association.
    MODEL_LOADED,    // The model has finished loading and can start
                     // associating now.
    ASSOCIATING,     // Model association is in progress.
    RUNNING,         // The controller is running and the data type is
                     // in sync with the cloud.
    STOPPING,        // The controller is in the process of stopping
                     // and is waiting for dependent services to stop.
    FAILED           // The controller was started but encountered an error.
  };

  // This enum is used for "Sync.*ConfigureFailre" histograms so the order
  // of is important. Any changes need to be reflected in histograms.xml.
  enum ConfigureResult {
    OK,                   // The data type has started normally.
    OK_FIRST_RUN,         // Same as OK, but sent on first successful
                          // start for this type for this user as
                          // determined by cloud state.
    ASSOCIATION_FAILED,   // An error occurred during model association.
    ABORTED,              // Start was aborted by calling Stop().
    UNRECOVERABLE_ERROR,  // An unrecoverable error occured.
    NEEDS_CRYPTO,         // The data type cannot be started yet because it
                          // depends on the cryptographer.
    RUNTIME_ERROR,        // After starting, a runtime error was encountered.
    MAX_CONFIGURE_RESULT
  };

  using StartCallback = base::Callback<
      void(ConfigureResult, const SyncMergeResult&, const SyncMergeResult&)>;

  using ModelLoadCallback = base::Callback<void(ModelType, const SyncError&)>;

  using StopCallback = base::OnceClosure;

  using AllNodesCallback =
      base::Callback<void(const ModelType, std::unique_ptr<base::ListValue>)>;

  using StatusCountersCallback =
      base::Callback<void(ModelType, const StatusCounters&)>;

  using TypeMap = std::map<ModelType, std::unique_ptr<DataTypeController>>;
  using TypeVector = std::vector<std::unique_ptr<DataTypeController>>;

  // Returns true if the start result should trigger an unrecoverable error.
  // Public so unit tests can use this function as well.
  static bool IsUnrecoverableResult(ConfigureResult result);

  // Returns true if the datatype started successfully.
  static bool IsSuccessfulResult(ConfigureResult result);

  static std::string StateToString(State state);

  virtual ~DataTypeController();

  // Returns true if DataTypeManager should wait for LoadModels to complete
  // successfully before starting configuration. Directory based types should
  // return false while USS datatypes should return true.
  virtual bool ShouldLoadModelBeforeConfigure() const = 0;

  // Called right before LoadModels. This method allows controller to register
  // the type with sync engine. Directory datatypes download initial data in
  // parallel with LoadModels and thus should be ready to receive updates with
  // initial data before LoadModels finishes.
  virtual void BeforeLoadModels(ModelTypeConfigurer* configurer) = 0;

  // Begins asynchronous operation of loading the model to get it ready for
  // model association. Once the models are loaded the callback will be invoked
  // with the result. If the models are already loaded it is safe to call the
  // callback right away. Else the callback needs to be stored and called when
  // the models are ready.
  virtual void LoadModels(const ConfigureContext& configure_context,
                          const ModelLoadCallback& model_load_callback) = 0;

  // Registers with sync backend if needed. This function is called by
  // DataTypeManager before downloading initial data. Non-blocking types need to
  // pass activation context containing progress marker to sync backend and use
  // |set_downloaded| to inform the manager whether their initial sync is done.
  virtual void RegisterWithBackend(base::Callback<void(bool)> set_downloaded,
                                   ModelTypeConfigurer* configurer) = 0;

  // Will start a potentially asynchronous operation to perform the
  // model association. Once the model association is done the callback will
  // be invoked.
  virtual void StartAssociating(const StartCallback& start_callback) = 0;

  // Called by DataTypeManager to activate the controlled data type using
  // one of the implementation specific methods provided by the |configurer|.
  // This is called (on UI thread) after the data type configuration has
  // completed successfully.
  virtual void ActivateDataType(ModelTypeConfigurer* configurer) = 0;

  // Called by DataTypeManager to deactivate the controlled data type.
  // See comments for ModelAssociationManager::OnSingleDataTypeWillStop.
  virtual void DeactivateDataType(ModelTypeConfigurer* configurer) = 0;

  // Stops the data type. If StartAssociating has already been called but is not
  // done yet it will be aborted. Similarly if LoadModels has not completed it
  // will also be aborted. Implementations may enter STOPPING state
  // transitionaly but should eventually become STOPPED. At this point,
  // |callback| will be run. |callback| must not be null.
  //
  // NOTE: Stop() should be called after sync backend machinery has stopped
  // routing changes to this data type. Stop() should ensure the data type
  // logic shuts down gracefully by flushing remaining changes and calling
  // StopSyncing on the SyncableService. This assumes no changes will ever
  // propagate from sync again from point where Stop() is called.
  // KEEP_METADATA is used when the engine just stops sync, and CLEAR_METADATA
  // is used when the user disables sync for data type.
  virtual void Stop(SyncStopMetadataFate metadata_fate,
                    StopCallback callback) = 0;

  // Name of this data type.  For logging purposes only.
  std::string name() const { return ModelTypeToString(type()); }

  // Current state of the data type controller.
  virtual State state() const = 0;

  // Unique model type for this data type controller.
  ModelType type() const { return type_; }

  // Whether the DataTypeController is ready to start. This is useful if the
  // datatype itself must make the decision about whether it should be enabled
  // at all (and therefore whether the initial download of the sync data for
  // the type should be performed).
  // Returns true by default.
  virtual bool ReadyForStart() const;

  // Returns a ListValue representing all nodes for this data type through
  // |callback| on this thread. Can only be called if state() != NOT_RUNNING.
  // Used for populating nodes in Sync Node Browser of chrome://sync-internals.
  virtual void GetAllNodes(const AllNodesCallback& callback) = 0;

  // Collects StatusCounters for this datatype and passes them to |callback|.
  // Used to display entity counts in chrome://sync-internals. Can be called
  // only if state() != NOT_RUNNING.
  virtual void GetStatusCounters(const StatusCountersCallback& callback) = 0;

  // Records entities count and estimated memory usage of the type into
  // histograms. Can be called only if state() != NOT_RUNNING.
  virtual void RecordMemoryUsageAndCountsHistograms() = 0;

 protected:
  explicit DataTypeController(ModelType type);

  // Allows subclasses to DCHECK that they're on the correct sequence.
  // TODO(crbug.com/846238): Rename this to CalledOnValidSequence.
  bool CalledOnValidThread() const;

 private:
  // The type this object is responsible for controlling.
  const ModelType type_;

  // Used to check that functions are called on the correct sequence.
  base::SequenceChecker sequence_checker_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_DATA_TYPE_CONTROLLER_H__
