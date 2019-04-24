// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_INTERNAL_PROTO_DATABASE_IMPL_H_
#define COMPONENTS_LEVELDB_PROTO_INTERNAL_PROTO_DATABASE_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/leveldb_proto/internal/proto_database_selector.h"
#include "components/leveldb_proto/internal/shared_proto_database.h"
#include "components/leveldb_proto/internal/shared_proto_database_provider.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/public/shared_proto_database_client_list.h"

namespace leveldb_proto {

class ProtoDatabaseProvider;

// Avoids circular dependencies between ProtoDatabaseImpl and
// ProtoDatabaseProvider, since we'd need to include the provider header here
// to use |db_provider_|'s GetSharedDBInstance.
void GetSharedDBInstance(
    ProtoDatabaseProvider* db_provider,
    base::OnceCallback<void(scoped_refptr<SharedProtoDatabase>)> callback);

// Update transactions happen on background task runner and callback runs on the
// client task runner.
void RunUpdateCallback(
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    Callbacks::UpdateCallback callback,
    bool success);

// Load transactions happen on background task runner. The loaded keys need to
// be given to clients on client task runner.
void RunLoadKeysCallback(
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    Callbacks::LoadKeysCallback callback,
    bool success,
    std::unique_ptr<KeyVector> keys);

// Helper to run destroy callback on the client task runner.
void RunDestroyCallback(
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    Callbacks::DestroyCallback callback,
    bool success);

// The ProtoDatabaseImpl<T> implements a ProtoDatabase<T> instance, and allows
// the underlying ProtoDatabase<T> implementation to change without users of the
// wrapper needing to know.
// This allows clients to request a DB instance without knowing whether or not
// it's a UniqueProtoDatabase or a SharedProtoDatabaseClient.
template <typename T>
class ProtoDatabaseImpl : public ProtoDatabase<T> {
 public:
  // DEPRECATED. Force usage of unique db. The clients must use Init(name,
  // db_dir, options, callback) version.
  ProtoDatabaseImpl(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  // Internal implementation is free to choose between unique and shared
  // database to use here (transparently).
  ProtoDatabaseImpl(ProtoDbType db_type,
                    const base::FilePath& db_dir,
                    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
                    std::unique_ptr<SharedProtoDatabaseProvider> db_provider);

  virtual ~ProtoDatabaseImpl() = default;

  // DEPRECATED. This is to be used in case of forced unique db behavior.
  void Init(const char* client_name,
            const base::FilePath& database_dir,
            const leveldb_env::Options& options,
            Callbacks::InitCallback callback) override;

  // This can be used along with a database obtained from new api
  // Provider::GetDB(). If using the old api to create unique db, then we do not
  // know the database dir and init will fail.
  void Init(const std::string& client_name,
            Callbacks::InitStatusCallback callback) override;

  // Internal only api.
  void InitWithDatabase(LevelDB* database,
                        const base::FilePath& database_dir,
                        const leveldb_env::Options& options,
                        Callbacks::InitStatusCallback callback);

  void UpdateEntries(std::unique_ptr<typename ProtoDatabase<T>::KeyEntryVector>
                         entries_to_save,
                     std::unique_ptr<KeyVector> keys_to_remove,
                     Callbacks::UpdateCallback callback) override;

  void UpdateEntriesWithRemoveFilter(
      std::unique_ptr<typename ProtoDatabase<T>::KeyEntryVector>
          entries_to_save,
      const KeyFilter& delete_key_filter,
      Callbacks::UpdateCallback callback) override;

  void LoadEntries(
      typename Callbacks::Internal<T>::LoadCallback callback) override;

  void LoadEntriesWithFilter(
      const KeyFilter& filter,
      typename Callbacks::Internal<T>::LoadCallback callback) override;
  void LoadEntriesWithFilter(
      const KeyFilter& key_filter,
      const leveldb::ReadOptions& options,
      const std::string& target_prefix,
      typename Callbacks::Internal<T>::LoadCallback callback) override;

  void LoadKeysAndEntries(
      typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback)
      override;

  void LoadKeysAndEntriesWithFilter(
      const KeyFilter& filter,
      typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback)
      override;
  void LoadKeysAndEntriesWithFilter(
      const KeyFilter& filter,
      const leveldb::ReadOptions& options,
      const std::string& target_prefix,
      typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback)
      override;
  void LoadKeysAndEntriesInRange(
      const std::string& start,
      const std::string& end,
      typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback)
      override;

  void LoadKeys(Callbacks::LoadKeysCallback callback) override;

  void GetEntry(const std::string& key,
                typename Callbacks::Internal<T>::GetCallback callback) override;

  void Destroy(Callbacks::DestroyCallback callback) override;

  void RemoveKeysForTesting(const KeyFilter& key_filter,
                            const std::string& target_prefix,
                            Callbacks::UpdateCallback callback);

  // Not thread safe.
  ProtoDatabaseSelector* db_wrapper_for_testing() { return db_wrapper_.get(); }

 private:
  friend class ProtoDatabaseImplTest;

  void Init(const std::string& client_name,
            bool use_shared_db,
            Callbacks::InitStatusCallback callback);

  void PostTransaction(base::OnceClosure task);

  ProtoDbType db_type_;
  scoped_refptr<ProtoDatabaseSelector> db_wrapper_;
  const bool force_unique_db_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::FilePath db_dir_;

  std::unique_ptr<base::WeakPtrFactory<ProtoDatabaseImpl<T>>> weak_ptr_factory_;
};

namespace {

// Update transactions need to serialize the entries to be updated on background
// task runner. The database can be accessed on same task runner. The caller
// must wrap the callback using RunUpdateCallback() to ensure the callback runs
// in client task runner.
template <typename T>
void UpdateEntriesFromTaskRunner(
    std::unique_ptr<typename Util::Internal<T>::KeyEntryVector> entries_to_save,
    std::unique_ptr<KeyVector> keys_to_remove,
    scoped_refptr<ProtoDatabaseSelector> db,
    Callbacks::UpdateCallback callback) {
  // Serialize the values from Proto to string before passing on to database.
  auto pairs_to_save = std::make_unique<KeyValueVector>();
  for (const auto& pair : *entries_to_save) {
    pairs_to_save->push_back(
        std::make_pair(pair.first, pair.second.SerializeAsString()));
  }

  db->UpdateEntries(std::move(pairs_to_save), std::move(keys_to_remove),
                    std::move(callback));
}

// Update transactions need to serialize the entries to be updated on background
// task runner. The database can be accessed on same task runner. The caller
// must wrap the callback using RunUpdateCallback() to ensure the callback runs
// in client task runner.
template <typename T>
void UpdateEntriesWithRemoveFilterFromTaskRunner(
    std::unique_ptr<typename ProtoDatabase<T>::KeyEntryVector> entries_to_save,
    const KeyFilter& delete_key_filter,
    scoped_refptr<ProtoDatabaseSelector> db,
    Callbacks::UpdateCallback callback) {
  // Serialize the values from Proto to string before passing on to database.
  auto pairs_to_save = std::make_unique<KeyValueVector>();
  for (const auto& pair : *entries_to_save) {
    pairs_to_save->push_back(
        std::make_pair(pair.first, pair.second.SerializeAsString()));
  }

  db->UpdateEntriesWithRemoveFilter(std::move(pairs_to_save), delete_key_filter,
                                    std::move(callback));
}

// Load transactions happen on background task runner. The loaded entries need
// to be parsed into proto in background thread. This wraps the load callback
// and parses the entries and posts result onto client task runner.
template <typename T>
void ParseLoadedEntries(
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    typename Callbacks::Internal<T>::LoadCallback callback,
    bool success,
    std::unique_ptr<ValueVector> loaded_entries) {
  auto entries = std::make_unique<std::vector<T>>();

  if (!success || !loaded_entries) {
    entries.reset();
  } else {
    for (const auto& serialized_entry : *loaded_entries) {
      T entry;
      if (!entry.ParseFromString(serialized_entry)) {
        DLOG(WARNING) << "Unable to parse leveldb_proto entry";
        // TODO(cjhopman): Decide what to do about un-parseable entries.
      }

      entries->push_back(entry);
    }
  }

  callback_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), success, std::move(entries)));
}

// Load transactions happen on background task runner. The loaded entries need
// to be parsed into proto in background thread. This wraps the load callback
// and parses the entries and posts result onto client task runner.
template <typename T>
void ParseLoadedKeysAndEntries(
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback,
    bool success,
    std::unique_ptr<KeyValueMap> loaded_entries) {
  auto keys_entries = std::make_unique<std::map<std::string, T>>();
  if (!success || !loaded_entries) {
    keys_entries.reset();
  } else {
    for (const auto& pair : *loaded_entries) {
      T entry;
      if (!entry.ParseFromString(pair.second)) {
        DLOG(WARNING) << "Unable to parse leveldb_proto entry";
        // TODO(cjhopman): Decide what to do about un-parseable entries.
      }

      keys_entries->insert(std::make_pair(pair.first, entry));
    }
  }

  callback_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), success, std::move(keys_entries)));
}

// Load transactions happen on background task runner. The loaded entries need
// to be parsed into proto in background thread. This wraps the load callback
// and parses the entries and posts result onto client task runner.
template <typename T>
void ParseLoadedEntry(
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    typename Callbacks::Internal<T>::GetCallback callback,
    bool success,
    std::unique_ptr<std::string> serialized_entry) {
  auto entry = std::make_unique<T>();

  if (!success || !serialized_entry) {
    entry.reset();
  } else if (!entry->ParseFromString(*serialized_entry)) {
    DLOG(WARNING) << "Unable to parse leveldb_proto entry";
    success = false;
    entry.reset();
  }
  callback_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), success, std::move(entry)));
}

}  // namespace

template <typename T>
ProtoDatabaseImpl<T>::ProtoDatabaseImpl(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : db_type_(ProtoDbType::LAST),
      db_wrapper_(new ProtoDatabaseSelector(db_type_, task_runner, nullptr)),
      force_unique_db_(true),
      task_runner_(task_runner),
      weak_ptr_factory_(
          std::make_unique<base::WeakPtrFactory<ProtoDatabaseImpl<T>>>(this)) {}

template <typename T>
ProtoDatabaseImpl<T>::ProtoDatabaseImpl(
    ProtoDbType db_type,
    const base::FilePath& db_dir,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    std::unique_ptr<SharedProtoDatabaseProvider> db_provider)
    : db_type_(db_type),
      db_wrapper_(new ProtoDatabaseSelector(db_type_,
                                            task_runner,
                                            std::move(db_provider))),
      force_unique_db_(false),
      task_runner_(task_runner),
      db_dir_(db_dir),
      weak_ptr_factory_(
          std::make_unique<base::WeakPtrFactory<ProtoDatabaseImpl<T>>>(this)) {}

template <typename T>
void ProtoDatabaseImpl<T>::Init(const char* client_uma_name,
                                const base::FilePath& database_dir,
                                const leveldb_env::Options& options,
                                Callbacks::InitCallback callback) {
  DCHECK(force_unique_db_);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ProtoDatabaseSelector::InitUniqueOrShared, db_wrapper_,
          client_uma_name, database_dir, options,
          /*use_shared_db=*/false, base::SequencedTaskRunnerHandle::Get(),
          base::BindOnce(
              [](Callbacks::InitCallback callback, Enums::InitStatus status) {
                std::move(callback).Run(status == Enums::InitStatus::kOK);
              },
              std::move(callback))));
}

template <typename T>
void ProtoDatabaseImpl<T>::Init(
    const std::string& client_uma_name,
    typename Callbacks::InitStatusCallback callback) {
  bool use_shared_db =
      !force_unique_db_ &&
      SharedProtoDatabaseClientList::ShouldUseSharedDB(db_type_);
  Init(client_uma_name, use_shared_db, std::move(callback));
}

template <typename T>
void ProtoDatabaseImpl<T>::Init(const std::string& client_name,
                                bool use_shared_db,
                                Callbacks::InitStatusCallback callback) {
  auto options = CreateSimpleOptions();
  // If we're NOT using a shared DB, we want to force creation of the unique one
  // because that's what we expect to be using moving forward. If we ARE using
  // the shared DB, we only want to see if there's a unique DB that already
  // exists and can be opened so that we can perform migration if necessary.
  options.create_if_missing = !use_shared_db;
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ProtoDatabaseSelector::InitUniqueOrShared, db_wrapper_,
                     client_name, db_dir_, options, use_shared_db,
                     base::SequencedTaskRunnerHandle::Get(),
                     std::move(callback)));
}

template <typename T>
void ProtoDatabaseImpl<T>::InitWithDatabase(
    LevelDB* database,
    const base::FilePath& database_dir,
    const leveldb_env::Options& options,
    Callbacks::InitStatusCallback callback) {
  DCHECK(force_unique_db_);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ProtoDatabaseSelector::InitWithDatabase, db_wrapper_,
                     base::Unretained(database), database_dir, options,
                     base::SequencedTaskRunnerHandle::Get(),
                     std::move(callback)));
}

template <typename T>
void ProtoDatabaseImpl<T>::UpdateEntries(
    std::unique_ptr<typename ProtoDatabase<T>::KeyEntryVector> entries_to_save,
    std::unique_ptr<KeyVector> keys_to_remove,
    Callbacks::UpdateCallback callback) {
  base::OnceClosure update_task = base::BindOnce(
      &UpdateEntriesFromTaskRunner<T>, std::move(entries_to_save),
      std::move(keys_to_remove), db_wrapper_,
      base::BindOnce(&RunUpdateCallback, base::SequencedTaskRunnerHandle::Get(),
                     std::move(callback)));
  PostTransaction(std::move(update_task));
}

template <typename T>
void ProtoDatabaseImpl<T>::UpdateEntriesWithRemoveFilter(
    std::unique_ptr<typename ProtoDatabase<T>::KeyEntryVector> entries_to_save,
    const KeyFilter& delete_key_filter,
    Callbacks::UpdateCallback callback) {
  base::OnceClosure update_task = base::BindOnce(
      &UpdateEntriesWithRemoveFilterFromTaskRunner<T>,
      std::move(entries_to_save), delete_key_filter, db_wrapper_,
      base::BindOnce(&RunUpdateCallback, base::SequencedTaskRunnerHandle::Get(),
                     std::move(callback)));
  PostTransaction(std::move(update_task));
}

template <typename T>
void ProtoDatabaseImpl<T>::LoadEntries(
    typename Callbacks::Internal<T>::LoadCallback callback) {
  LoadEntriesWithFilter(KeyFilter(), std::move(callback));
}

template <typename T>
void ProtoDatabaseImpl<T>::LoadEntriesWithFilter(
    const KeyFilter& filter,
    typename Callbacks::Internal<T>::LoadCallback callback) {
  LoadEntriesWithFilter(filter, leveldb::ReadOptions(), std::string(),
                        std::move(callback));
}

template <typename T>
void ProtoDatabaseImpl<T>::LoadEntriesWithFilter(
    const KeyFilter& key_filter,
    const leveldb::ReadOptions& options,
    const std::string& target_prefix,
    typename Callbacks::Internal<T>::LoadCallback callback) {
  base::OnceClosure load_task =
      base::BindOnce(&ProtoDatabaseSelector::LoadEntriesWithFilter, db_wrapper_,
                     key_filter, options, target_prefix,
                     base::BindOnce(&ParseLoadedEntries<T>,
                                    base::SequencedTaskRunnerHandle::Get(),
                                    std::move(callback)));
  PostTransaction(std::move(load_task));
}

template <typename T>
void ProtoDatabaseImpl<T>::LoadKeysAndEntries(
    typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback) {
  LoadKeysAndEntriesWithFilter(KeyFilter(), std::move(callback));
}

template <typename T>
void ProtoDatabaseImpl<T>::LoadKeysAndEntriesWithFilter(
    const KeyFilter& filter,
    typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback) {
  LoadKeysAndEntriesWithFilter(filter, leveldb::ReadOptions(), std::string(),
                               std::move(callback));
}

template <typename T>
void ProtoDatabaseImpl<T>::LoadKeysAndEntriesWithFilter(
    const KeyFilter& filter,
    const leveldb::ReadOptions& options,
    const std::string& target_prefix,
    typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback) {
  base::OnceClosure load_task =
      base::BindOnce(&ProtoDatabaseSelector::LoadKeysAndEntriesWithFilter,
                     db_wrapper_, filter, options, target_prefix,
                     base::BindOnce(&ParseLoadedKeysAndEntries<T>,
                                    base::SequencedTaskRunnerHandle::Get(),
                                    std::move(callback)));
  PostTransaction(std::move(load_task));
}

template <typename T>
void ProtoDatabaseImpl<T>::LoadKeysAndEntriesInRange(
    const std::string& start,
    const std::string& end,
    typename Callbacks::Internal<T>::LoadKeysAndEntriesCallback callback) {
  base::OnceClosure load_task =
      base::BindOnce(&ProtoDatabaseSelector::LoadKeysAndEntriesInRange,
                     db_wrapper_, start, end,
                     base::BindOnce(&ParseLoadedKeysAndEntries<T>,
                                    base::SequencedTaskRunnerHandle::Get(),
                                    std::move(callback)));
  PostTransaction(std::move(load_task));
}

template <typename T>
void ProtoDatabaseImpl<T>::LoadKeys(Callbacks::LoadKeysCallback callback) {
  base::OnceClosure load_task =
      base::BindOnce(&ProtoDatabaseSelector::LoadKeys, db_wrapper_,
                     base::BindOnce(&RunLoadKeysCallback,
                                    base::SequencedTaskRunnerHandle::Get(),
                                    std::move(callback)));
  PostTransaction(std::move(load_task));
}

template <typename T>
void ProtoDatabaseImpl<T>::GetEntry(
    const std::string& key,
    typename Callbacks::Internal<T>::GetCallback callback) {
  base::OnceClosure get_task =
      base::BindOnce(&ProtoDatabaseSelector::GetEntry, db_wrapper_, key,
                     base::BindOnce(&ParseLoadedEntry<T>,
                                    base::SequencedTaskRunnerHandle::Get(),
                                    std::move(callback)));
  PostTransaction(std::move(get_task));
}

template <typename T>
void ProtoDatabaseImpl<T>::Destroy(Callbacks::DestroyCallback callback) {
  base::OnceClosure destroy_task =
      base::BindOnce(&ProtoDatabaseSelector::Destroy, db_wrapper_,
                     base::BindOnce(&RunDestroyCallback,
                                    base::SequencedTaskRunnerHandle::Get(),
                                    std::move(callback)));
  PostTransaction(std::move(destroy_task));
}

template <typename T>
void ProtoDatabaseImpl<T>::RemoveKeysForTesting(
    const KeyFilter& key_filter,
    const std::string& target_prefix,
    Callbacks::UpdateCallback callback) {
  base::OnceClosure update_task = base::BindOnce(
      &ProtoDatabaseSelector::RemoveKeysForTesting, db_wrapper_, key_filter,
      target_prefix,
      base::BindOnce(&RunUpdateCallback, base::SequencedTaskRunnerHandle::Get(),
                     std::move(callback)));
  PostTransaction(std::move(update_task));
}

template <typename T>
void ProtoDatabaseImpl<T>::PostTransaction(base::OnceClosure task) {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&ProtoDatabaseSelector::AddTransaction,
                                        db_wrapper_, std::move(task)));
}

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_INTERNAL_PROTO_DATABASE_IMPL_H_