// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_SYNC_MANAGER_H_
#define SYNC_INTERNAL_API_PUBLIC_SYNC_MANAGER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner.h"
#include "base/threading/thread_checker.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/change_record.h"
#include "sync/internal_api/public/configure_reason.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"
#include "sync/internal_api/public/engine/sync_status.h"
#include "sync/internal_api/public/sync_encryption_handler.h"
#include "sync/internal_api/public/util/report_unrecoverable_error_function.h"
#include "sync/internal_api/public/util/weak_handle.h"
#include "sync/notifier/invalidation_util.h"
#include "sync/protocol/sync_protocol_error.h"

namespace sync_pb {
class EncryptedData;
}  // namespace sync_pb

namespace syncer {

class BaseTransaction;
class DataTypeDebugInfoListener;
class Encryptor;
struct Experiments;
class ExtensionsActivityMonitor;
class HttpPostProviderFactory;
class InternalComponentsFactory;
class InvalidationHandler;
class Invalidator;
class JsBackend;
class JsEventHandler;
class SyncEncryptionHandler;
class SyncScheduler;
class UnrecoverableErrorHandler;
struct UserShare;

namespace sessions {
class SyncSessionSnapshot;
}  // namespace sessions

// Used by SyncManager::OnConnectionStatusChange().
enum ConnectionStatus {
  CONNECTION_OK,
  CONNECTION_AUTH_ERROR,
  CONNECTION_SERVER_ERROR
};

// Contains everything needed to talk to and identify a user account.
struct SyncCredentials {
  std::string email;
  std::string sync_token;
};

// SyncManager encapsulates syncable::Directory and serves as the parent of all
// other objects in the sync API.  If multiple threads interact with the same
// local sync repository (i.e. the same sqlite database), they should share a
// single SyncManager instance.  The caller should typically create one
// SyncManager for the lifetime of a user session.
//
// Unless stated otherwise, all methods of SyncManager should be called on the
// same thread.
class SYNC_EXPORT SyncManager {
 public:
  // An interface the embedding application implements to be notified
  // on change events.  Note that these methods may be called on *any*
  // thread.
  class SYNC_EXPORT ChangeDelegate {
   public:
    // Notify the delegate that changes have been applied to the sync model.
    //
    // This will be invoked on the same thread as on which ApplyChanges was
    // called. |changes| is an array of size |change_count|, and contains the
    // ID of each individual item that was changed. |changes| exists only for
    // the duration of the call. If items of multiple data types change at
    // the same time, this method is invoked once per data type and |changes|
    // is restricted to items of the ModelType indicated by |model_type|.
    // Because the observer is passed a |trans|, the observer can assume a
    // read lock on the sync model that will be released after the function
    // returns.
    //
    // The SyncManager constructs |changes| in the following guaranteed order:
    //
    // 1. Deletions, from leaves up to parents.
    // 2. Updates to existing items with synced parents & predecessors.
    // 3. New items with synced parents & predecessors.
    // 4. Items with parents & predecessors in |changes|.
    // 5. Repeat #4 until all items are in |changes|.
    //
    // Thus, an implementation of OnChangesApplied should be able to
    // process the change records in the order without having to worry about
    // forward dependencies.  But since deletions come before reparent
    // operations, a delete may temporarily orphan a node that is
    // updated later in the list.
    virtual void OnChangesApplied(
        ModelType model_type,
        int64 model_version,
        const BaseTransaction* trans,
        const ImmutableChangeRecordList& changes) = 0;

    // OnChangesComplete gets called when the TransactionComplete event is
    // posted (after OnChangesApplied finishes), after the transaction lock
    // and the change channel mutex are released.
    //
    // The purpose of this function is to support processors that require
    // split-transactions changes. For example, if a model processor wants to
    // perform blocking I/O due to a change, it should calculate the changes
    // while holding the transaction lock (from within OnChangesApplied), buffer
    // those changes, let the transaction fall out of scope, and then commit
    // those changes from within OnChangesComplete (postponing the blocking
    // I/O to when it no longer holds any lock).
    virtual void OnChangesComplete(ModelType model_type) = 0;

   protected:
    virtual ~ChangeDelegate();
  };

  // Like ChangeDelegate, except called only on the sync thread and
  // not while a transaction is held.  For objects that want to know
  // when changes happen, but don't need to process them.
  class SYNC_EXPORT_PRIVATE ChangeObserver {
   public:
    // Ids referred to in |changes| may or may not be in the write
    // transaction specified by |write_transaction_id|.  If they're
    // not, that means that the node didn't actually change, but we
    // marked them as changed for some other reason (e.g., siblings of
    // re-ordered nodes).
    //
    // TODO(sync, long-term): Ideally, ChangeDelegate/Observer would
    // be passed a transformed version of EntryKernelMutation instead
    // of a transaction that would have to be used to look up the
    // changed nodes.  That is, ChangeDelegate::OnChangesApplied()
    // would still be called under the transaction, but all the needed
    // data will be passed down.
    //
    // Even more ideally, we would have sync semantics such that we'd
    // be able to apply changes without being under a transaction.
    // But that's a ways off...
    virtual void OnChangesApplied(
        ModelType model_type,
        int64 write_transaction_id,
        const ImmutableChangeRecordList& changes) = 0;

    virtual void OnChangesComplete(ModelType model_type) = 0;

   protected:
    virtual ~ChangeObserver();
  };

  // An interface the embedding application implements to receive
  // notifications from the SyncManager.  Register an observer via
  // SyncManager::AddObserver.  All methods are called only on the
  // sync thread.
  class SYNC_EXPORT Observer {
   public:
    // A round-trip sync-cycle took place and the syncer has resolved any
    // conflicts that may have arisen.
    virtual void OnSyncCycleCompleted(
        const sessions::SyncSessionSnapshot& snapshot) = 0;

    // Called when the status of the connection to the sync server has
    // changed.
    virtual void OnConnectionStatusChange(ConnectionStatus status) = 0;

    // Called when a new auth token is provided by the sync server.
    virtual void OnUpdatedToken(const std::string& token) = 0;

    // Called when initialization is complete to the point that SyncManager can
    // process changes. This does not necessarily mean authentication succeeded
    // or that the SyncManager is online.
    // IMPORTANT: Creating any type of transaction before receiving this
    // notification is illegal!
    // WARNING: Calling methods on the SyncManager before receiving this
    // message, unless otherwise specified, produces undefined behavior.
    //
    // |js_backend| is what about:sync interacts with.  It can emit
    // the following events:

    /**
     * @param {{ enabled: boolean }} details A dictionary containing:
     *     - enabled: whether or not notifications are enabled.
     */
    // function onNotificationStateChange(details);

    /**
     * @param {{ changedTypes: Array.<string> }} details A dictionary
     *     containing:
     *     - changedTypes: a list of types (as strings) for which there
             are new updates.
     */
    // function onIncomingNotification(details);

    // Also, it responds to the following messages (all other messages
    // are ignored):

    /**
     * Gets the current notification state.
     *
     * @param {function(boolean)} callback Called with whether or not
     *     notifications are enabled.
     */
    // function getNotificationState(callback);

    /**
     * Gets details about the root node.
     *
     * @param {function(!Object)} callback Called with details about the
     *     root node.
     */
    // TODO(akalin): Change this to getRootNodeId or eliminate it
    // entirely.
    // function getRootNodeDetails(callback);

    /**
     * Gets summary information for a list of ids.
     *
     * @param {Array.<string>} idList List of 64-bit ids in decimal
     *     string form.
     * @param {Array.<{id: string, title: string, isFolder: boolean}>}
     * callback Called with summaries for the nodes in idList that
     *     exist.
     */
    // function getNodeSummariesById(idList, callback);

    /**
     * Gets detailed information for a list of ids.
     *
     * @param {Array.<string>} idList List of 64-bit ids in decimal
     *     string form.
     * @param {Array.<!Object>} callback Called with detailed
     *     information for the nodes in idList that exist.
     */
    // function getNodeDetailsById(idList, callback);

    /**
     * Gets child ids for a given id.
     *
     * @param {string} id 64-bit id in decimal string form of the parent
     *     node.
     * @param {Array.<string>} callback Called with the (possibly empty)
     *     list of child ids.
     */
    // function getChildNodeIds(id);

    virtual void OnInitializationComplete(
        const WeakHandle<JsBackend>& js_backend,
        const WeakHandle<DataTypeDebugInfoListener>& debug_info_listener,
        bool success,
        ModelTypeSet restored_types) = 0;

    // We are no longer permitted to communicate with the server. Sync should
    // be disabled and state cleaned up at once.  This can happen for a number
    // of reasons, e.g. swapping from a test instance to production, or a
    // global stop syncing operation has wiped the store.
    virtual void OnStopSyncingPermanently() = 0;

    virtual void OnActionableError(
        const SyncProtocolError& sync_protocol_error) = 0;

   protected:
    virtual ~Observer();
  };

  SyncManager();
  virtual ~SyncManager();

  // Initialize the sync manager.  |database_location| specifies the path of
  // the directory in which to locate a sqlite repository storing the syncer
  // backend state. Initialization will open the database, or create it if it
  // does not already exist. Returns false on failure.
  // |event_handler| is the JsEventHandler used to propagate events to
  // chrome://sync-internals.  |event_handler| may be uninitialized.
  // |sync_server_and_path| and |sync_server_port| represent the Chrome sync
  // server to use, and |use_ssl| specifies whether to communicate securely;
  // the default is false.
  // |post_factory| will be owned internally and used to create
  // instances of an HttpPostProvider.
  // |model_safe_worker| ownership is given to the SyncManager.
  // |user_agent| is a 7-bit ASCII string suitable for use as the User-Agent
  // HTTP header. Used internally when collecting stats to classify clients.
  // |invalidator| is owned and used to listen for invalidations.
  // |restored_key_for_bootstrapping| is the key used to boostrap the
  // cryptographer
  // |keystore_encryption_enabled| determines whether we enable the keystore
  // encryption functionality in the cryptographer/nigori.
  // |report_unrecoverable_error_function| may be NULL.
  //
  // TODO(akalin): Replace the |post_factory| parameter with a
  // URLFetcher parameter.
  virtual void Init(
      const base::FilePath& database_location,
      const WeakHandle<JsEventHandler>& event_handler,
      const std::string& sync_server_and_path,
      int sync_server_port,
      bool use_ssl,
      scoped_ptr<HttpPostProviderFactory> post_factory,
      const std::vector<ModelSafeWorker*>& workers,
      ExtensionsActivityMonitor* extensions_activity_monitor,
      ChangeDelegate* change_delegate,
      const SyncCredentials& credentials,
      scoped_ptr<Invalidator> invalidator,
      const std::string& restored_key_for_bootstrapping,
      const std::string& restored_keystore_key_for_bootstrapping,
      scoped_ptr<InternalComponentsFactory> internal_components_factory,
      Encryptor* encryptor,
      UnrecoverableErrorHandler* unrecoverable_error_handler,
      ReportUnrecoverableErrorFunction report_unrecoverable_error_function) = 0;

  // Throw an unrecoverable error from a transaction (mostly used for
  // testing).
  virtual void ThrowUnrecoverableError() = 0;

  virtual ModelTypeSet InitialSyncEndedTypes() = 0;

  // Returns those types within |types| that have an empty progress marker
  // token.
  virtual ModelTypeSet GetTypesWithEmptyProgressMarkerToken(
      ModelTypeSet types) = 0;

  // Purge from the directory those types with non-empty progress markers
  // but without initial synced ended set.
  // Returns false if an error occurred, true otherwise.
  virtual bool PurgePartiallySyncedTypes() = 0;

  // Update tokens that we're using in Sync. Email must stay the same.
  virtual void UpdateCredentials(const SyncCredentials& credentials) = 0;

  // Called when the user disables or enables a sync type.
  virtual void UpdateEnabledTypes(ModelTypeSet enabled_types) = 0;

  // Forwards to the underlying invalidator (see comments in invalidator.h).
  virtual void RegisterInvalidationHandler(
      InvalidationHandler* handler) = 0;

  // Forwards to the underlying notifier (see comments in invalidator.h).
  virtual void UpdateRegisteredInvalidationIds(
      InvalidationHandler* handler,
      const ObjectIdSet& ids) = 0;

  // Forwards to the underlying notifier (see comments in invalidator.h).
  virtual void UnregisterInvalidationHandler(
      InvalidationHandler* handler) = 0;

  // Put the syncer in normal mode ready to perform nudges and polls.
  virtual void StartSyncingNormally(
      const ModelSafeRoutingInfo& routing_info) = 0;

  // Switches the mode of operation to CONFIGURATION_MODE and performs
  // any configuration tasks needed as determined by the params. Once complete,
  // syncer will remain in CONFIGURATION_MODE until StartSyncingNormally is
  // called.
  // Data whose types are not in |new_routing_info| are purged from sync
  // directory. The purged data is backed up in delete journal for recovery in
  // next session if its type is in |failed_types|.
  // |ready_task| is invoked when the configuration completes.
  // |retry_task| is invoked if the configuration job could not immediately
  //              execute. |ready_task| will still be called when it eventually
  //              does finish.
  virtual void ConfigureSyncer(
      ConfigureReason reason,
      ModelTypeSet types_to_config,
      ModelTypeSet failed_types,
      const ModelSafeRoutingInfo& new_routing_info,
      const base::Closure& ready_task,
      const base::Closure& retry_task) = 0;

  // Adds a listener to be notified of sync events.
  // NOTE: It is OK (in fact, it's probably a good idea) to call this before
  // having received OnInitializationCompleted.
  virtual void AddObserver(Observer* observer) = 0;

  // Remove the given observer.  Make sure to call this if the
  // Observer is being destroyed so the SyncManager doesn't
  // potentially dereference garbage.
  virtual void RemoveObserver(Observer* observer) = 0;

  // Status-related getter.  May be called on any thread.
  virtual SyncStatus GetDetailedStatus() const = 0;

  // Call periodically from a database-safe thread to persist recent changes
  // to the syncapi model.
  virtual void SaveChanges() = 0;

  // Initiates shutdown of various components in the sync engine.  Must be
  // called from the main thread to allow preempting ongoing tasks on the sync
  // loop (that may be blocked on I/O).  The semantics of |callback| are the
  // same as with StartConfigurationMode. If provided and a scheduler / sync
  // loop exists, it will be invoked from the sync loop by the scheduler to
  // notify that all work has been flushed + cancelled, and it is idle.
  // If no scheduler exists, the callback is run immediately (from the loop
  // this was created on, which is the sync loop), as sync is effectively
  // stopped.
  virtual void StopSyncingForShutdown(const base::Closure& callback) = 0;

  // Issue a final SaveChanges, and close sqlite handles.
  virtual void ShutdownOnSyncThread() = 0;

  // May be called from any thread.
  virtual UserShare* GetUserShare() = 0;

  // Returns the cache_guid of the currently open database.
  // Requires that the SyncManager be initialized.
  virtual const std::string cache_guid() = 0;

  // Reads the nigori node to determine if any experimental features should
  // be enabled.
  // Note: opens a transaction.  May be called on any thread.
  virtual bool ReceivedExperiment(Experiments* experiments) = 0;

  // Uses a read-only transaction to determine if the directory being synced has
  // any remaining unsynced items.  May be called on any thread.
  virtual bool HasUnsyncedItems() = 0;

  // Returns the SyncManager's encryption handler.
  virtual SyncEncryptionHandler* GetEncryptionHandler() = 0;

  // Ask the SyncManager to fetch updates for the given types.
  virtual void RefreshTypes(ModelTypeSet types) = 0;
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_SYNC_MANAGER_H_
