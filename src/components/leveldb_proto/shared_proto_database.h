// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_SHARED_PROTO_DATABASE_H_
#define COMPONENTS_LEVELDB_PROTO_SHARED_PROTO_DATABASE_H_

#include <queue>

#include "base/bind_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "components/leveldb_proto/leveldb_database.h"
#include "components/leveldb_proto/proto_database.h"
#include "components/leveldb_proto/proto_leveldb_wrapper.h"
#include "components/leveldb_proto/shared_proto_database_client.h"

namespace leveldb_proto {

// Controls a single LevelDB database to be used by many clients, and provides
// a way to get SharedProtoDatabaseClients that allow shared access to the
// underlying single database.
class SharedProtoDatabase : public base::RefCounted<SharedProtoDatabase> {
 public:
  void GetDatabaseInitStateAsync(ProtoLevelDBWrapper::InitCallback callback);

  // Always returns a SharedProtoDatabaseClient pointer, but that should ONLY
  // be used if the callback returns success.
  template <typename T>
  std::unique_ptr<SharedProtoDatabaseClient<T>> GetClient(
      const std::string& client_namespace,
      const std::string& type_prefix,
      bool create_if_missing,
      ProtoLevelDBWrapper::InitCallback callback);

 private:
  friend class base::RefCounted<SharedProtoDatabase>;
  friend class ProtoDatabaseProvider;

  friend class SharedProtoDatabaseTest;
  friend class SharedProtoDatabaseClientTest;

  enum InitState {
    kNone,
    kInProgress,
    kSuccess,
    kFailure,
  };

  // Private since we only want to create a singleton of it.
  SharedProtoDatabase(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      const std::string& client_name,
      const base::FilePath& db_dir);

  virtual ~SharedProtoDatabase();

  // |callback_task_runner| should be the same sequence that Init was called
  // from.
  void Init(bool create_if_missing,
            ProtoLevelDBWrapper::InitCallback callback,
            scoped_refptr<base::SequencedTaskRunner> callback_task_runner);
  void OnDatabaseInit(
      ProtoLevelDBWrapper::InitCallback callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      bool success);
  void RunInitCallback(ProtoLevelDBWrapper::InitCallback callback);

  LevelDB* GetLevelDBForTesting() const;

  SEQUENCE_CHECKER(on_creation_sequence_);
  SEQUENCE_CHECKER(on_task_runner_);

  InitState init_state_ = InitState::kNone;

  // This TaskRunner is used to properly sequence Init calls and checks for the
  // current init state. When clients request the current InitState as part of
  // their call to their Init function, the request is put into this TaskRunner.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::FilePath db_dir_;
  std::unique_ptr<ProtoLevelDBWrapper> db_wrapper_;
  std::unique_ptr<LevelDB> db_;

  base::WeakPtrFactory<SharedProtoDatabase> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SharedProtoDatabase);
};

template <typename T>
std::unique_ptr<SharedProtoDatabaseClient<T>> SharedProtoDatabase::GetClient(
    const std::string& client_namespace,
    const std::string& type_prefix,
    bool create_if_missing,
    ProtoLevelDBWrapper::InitCallback callback) {
  DCHECK(base::SequencedTaskRunnerHandle::IsSet());
  auto current_task_runner = base::SequencedTaskRunnerHandle::Get();
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SharedProtoDatabase::Init, weak_factory_.GetWeakPtr(),
                     create_if_missing, std::move(callback),
                     std::move(current_task_runner)));
  return base::WrapUnique(new SharedProtoDatabaseClient<T>(
      std::make_unique<ProtoLevelDBWrapper>(task_runner_, db_.get()),
      client_namespace, type_prefix, this));
}

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_SHARED_PROTO_DATABASE_H_
