// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_MODEL_TYPE_REGISTRY_H_
#define SYNC_ENGINE_MODEL_TYPE_REGISTRY_H_

#include <map>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "sync/base/sync_export.h"
#include "sync/engine/nudge_handler.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"
#include "sync/internal_api/public/non_blocking_sync_common.h"
#include "sync/internal_api/public/sessions/type_debug_info_observer.h"
#include "sync/internal_api/public/sync_context.h"
#include "sync/internal_api/public/sync_encryption_handler.h"

namespace syncer {

namespace syncable {
class Directory;
}  // namespace syncable

class CommitContributor;
class DirectoryCommitContributor;
class DirectoryUpdateHandler;
class DirectoryTypeDebugInfoEmitter;
class ModelTypeSyncWorkerImpl;
class ModelTypeSyncProxyImpl;
class UpdateHandler;

typedef std::map<ModelType, UpdateHandler*> UpdateHandlerMap;
typedef std::map<ModelType, CommitContributor*> CommitContributorMap;
typedef std::map<ModelType, DirectoryTypeDebugInfoEmitter*>
    DirectoryTypeDebugInfoEmitterMap;

// Keeps track of the sets of active update handlers and commit contributors.
class SYNC_EXPORT_PRIVATE ModelTypeRegistry
    : public SyncContext,
      public SyncEncryptionHandler::Observer {
 public:
  // Constructs a ModelTypeRegistry that supports directory types.
  ModelTypeRegistry(const std::vector<scoped_refptr<ModelSafeWorker> >& workers,
                    syncable::Directory* directory,
                    NudgeHandler* nudge_handler);
  virtual ~ModelTypeRegistry();

  // Sets the set of enabled types.
  void SetEnabledDirectoryTypes(const ModelSafeRoutingInfo& routing_info);

  // Enables an off-thread type for syncing.  Connects the given proxy
  // and its task_runner to the newly created worker.
  //
  // Expects that the proxy's ModelType is not currently enabled.
  virtual void ConnectSyncTypeToWorker(
      syncer::ModelType type,
      const DataTypeState& data_type_state,
      const syncer::UpdateResponseDataList& saved_pending_updates,
      const scoped_refptr<base::SequencedTaskRunner>& type_task_runner,
      const base::WeakPtr<ModelTypeSyncProxyImpl>& proxy) OVERRIDE;

  // Disables the syncing of an off-thread type.
  //
  // Expects that the type is currently enabled.
  // Deletes the worker associated with the type.
  virtual void DisconnectSyncWorker(syncer::ModelType type) OVERRIDE;

  // Implementation of SyncEncryptionHandler::Observer.
  virtual void OnPassphraseRequired(
      PassphraseRequiredReason reason,
      const sync_pb::EncryptedData& pending_keys) OVERRIDE;
  virtual void OnPassphraseAccepted() OVERRIDE;
  virtual void OnBootstrapTokenUpdated(const std::string& bootstrap_token,
                                       BootstrapTokenType type) OVERRIDE;
  virtual void OnEncryptedTypesChanged(ModelTypeSet encrypted_types,
                                       bool encrypt_everything) OVERRIDE;
  virtual void OnEncryptionComplete() OVERRIDE;
  virtual void OnCryptographerStateChanged(
      Cryptographer* cryptographer) OVERRIDE;
  virtual void OnPassphraseTypeChanged(PassphraseType type,
                                       base::Time passphrase_time) OVERRIDE;

  // Gets the set of enabled types.
  ModelTypeSet GetEnabledTypes() const;

  // Simple getters.
  UpdateHandlerMap* update_handler_map();
  CommitContributorMap* commit_contributor_map();
  DirectoryTypeDebugInfoEmitterMap* directory_type_debug_info_emitter_map();

  void RegisterDirectoryTypeDebugInfoObserver(
      syncer::TypeDebugInfoObserver* observer);
  void UnregisterDirectoryTypeDebugInfoObserver(
      syncer::TypeDebugInfoObserver* observer);
  bool HasDirectoryTypeDebugInfoObserver(
      syncer::TypeDebugInfoObserver* observer);
  void RequestEmitDebugInfo();

  base::WeakPtr<SyncContext> AsWeakPtr();

 private:
  void OnEncryptionStateChanged();

  ModelTypeSet GetEnabledNonBlockingTypes() const;
  ModelTypeSet GetEnabledDirectoryTypes() const;

  // Sets of handlers and contributors.
  ScopedVector<DirectoryCommitContributor> directory_commit_contributors_;
  ScopedVector<DirectoryUpdateHandler> directory_update_handlers_;
  ScopedVector<DirectoryTypeDebugInfoEmitter>
      directory_type_debug_info_emitters_;

  ScopedVector<ModelTypeSyncWorkerImpl> model_type_sync_workers_;

  // Maps of UpdateHandlers and CommitContributors.
  // They do not own any of the objects they point to.
  UpdateHandlerMap update_handler_map_;
  CommitContributorMap commit_contributor_map_;

  // Map of DebugInfoEmitters for directory types.
  // Non-blocking types handle debug info differently.
  // Does not own its contents.
  DirectoryTypeDebugInfoEmitterMap directory_type_debug_info_emitter_map_;

  // The known ModelSafeWorkers.
  std::map<ModelSafeGroup, scoped_refptr<ModelSafeWorker> > workers_map_;

  // The directory.  Not owned.
  syncable::Directory* directory_;

  // A copy of the directory's most recent cryptographer.
  scoped_ptr<Cryptographer> cryptographer_;

  // The set of encrypted types.
  ModelTypeSet encrypted_types_;

  // A helper that manages cryptography state and preferences.
  SyncEncryptionHandler* encryption_handler_;

  // The NudgeHandler.  Not owned.
  NudgeHandler* nudge_handler_;

  // The set of enabled directory types.
  ModelTypeSet enabled_directory_types_;

  // The set of observers of per-type debug info.
  //
  // Each of the DirectoryTypeDebugInfoEmitters needs such a list.  There's
  // a lot of them, and their lifetimes are unpredictable, so it makes the
  // book-keeping easier if we just store the list here.  That way it's
  // guaranteed to live as long as this sync backend.
  ObserverList<TypeDebugInfoObserver> type_debug_info_observers_;

  base::WeakPtrFactory<ModelTypeRegistry> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ModelTypeRegistry);
};

}  // namespace syncer

#endif // SYNC_ENGINE_MODEL_TYPE_REGISTRY_H_
