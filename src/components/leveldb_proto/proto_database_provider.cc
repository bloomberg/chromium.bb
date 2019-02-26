// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/proto_database_provider.h"

#include "base/files/file_path.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"
#include "components/leveldb_proto/shared_proto_database.h"

namespace {

const char kSharedProtoDatabaseClientName[] = "SharedProtoDB";
const char kSharedProtoDatabaseDirectory[] = "shared_proto_db";

}  // namespace

namespace leveldb_proto {

ProtoDatabaseProvider::ProtoDatabaseProvider(const base::FilePath& profile_dir)
    : profile_dir_(profile_dir),
      task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
      weak_factory_(this) {}

ProtoDatabaseProvider::~ProtoDatabaseProvider() = default;

// static
ProtoDatabaseProvider* ProtoDatabaseProvider::Create(
    const base::FilePath& profile_dir) {
  return new ProtoDatabaseProvider(profile_dir);
}

void ProtoDatabaseProvider::GetSharedDBInstance(
    GetSharedDBInstanceCallback callback) {
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          &ProtoDatabaseProvider::PrepareSharedDBInstanceOnTaskRunner,
          weak_factory_.GetWeakPtr()),
      base::BindOnce(&ProtoDatabaseProvider::RunGetSharedDBInstanceCallback,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ProtoDatabaseProvider::PrepareSharedDBInstanceOnTaskRunner() {
  if (db_)
    return;

  db_ = base::WrapRefCounted(new SharedProtoDatabase(
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}),
      kSharedProtoDatabaseClientName,
      profile_dir_.AppendASCII(std::string(kSharedProtoDatabaseDirectory))));
}

void ProtoDatabaseProvider::RunGetSharedDBInstanceCallback(
    GetSharedDBInstanceCallback callback) {
  std::move(callback).Run(db_);
}

}  // namespace leveldb_proto
