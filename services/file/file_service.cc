// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/file/file_service.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/task/post_task.h"
#include "components/services/filesystem/lock_table.h"
#include "components/services/leveldb/leveldb_service_impl.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "services/file/file_system.h"

namespace file {

class FileService::FileSystemObjects
    : public base::SupportsWeakPtr<FileSystemObjects> {
 public:
  // Created on the main thread.
  FileSystemObjects(base::FilePath user_dir) : user_dir_(user_dir) {}

  // Destroyed on the |file_service_runner_|.
  ~FileSystemObjects() = default;

  // Called on the |file_service_runner_|.
  void BindFileSystemReceiver(
      mojo::PendingReceiver<mojom::FileSystem> receiver) {
    if (!lock_table_)
      lock_table_ = new filesystem::LockTable;
    file_system_receivers_.Add(
        std::make_unique<FileSystem>(user_dir_, lock_table_),
        std::move(receiver));
  }

 private:
  scoped_refptr<filesystem::LockTable> lock_table_;
  base::FilePath user_dir_;
  mojo::UniqueReceiverSet<mojom::FileSystem> file_system_receivers_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemObjects);
};

class FileService::LevelDBServiceObjects
    : public base::SupportsWeakPtr<LevelDBServiceObjects> {
 public:
  // Created on the main thread.
  LevelDBServiceObjects(
      scoped_refptr<base::SequencedTaskRunner> file_task_runner)
      : file_task_runner_(std::move(file_task_runner)) {}

  // Destroyed on the |leveldb_service_runner_|.
  ~LevelDBServiceObjects() = default;

  // Called on the |leveldb_service_runner_|.
  void BindLevelDBServiceReceiver(
      mojo::PendingReceiver<leveldb::mojom::LevelDBService> receiver) {
    if (!leveldb_service_) {
      leveldb_service_ =
          std::make_unique<leveldb::LevelDBServiceImpl>(file_task_runner_);
    }
    leveldb_receivers_.Add(leveldb_service_.get(), std::move(receiver));
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  // Variables that are only accessible on the |leveldb_service_runner_| thread.
  std::unique_ptr<leveldb::mojom::LevelDBService> leveldb_service_;
  mojo::ReceiverSet<leveldb::mojom::LevelDBService> leveldb_receivers_;

  DISALLOW_COPY_AND_ASSIGN(LevelDBServiceObjects);
};

FileService::FileService(const base::FilePath& directory)
    : file_service_runner_(base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      leveldb_service_runner_(base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})) {
  if (!directory.empty()) {
    DCHECK(directory.IsAbsolute());
    file_system_objects_ =
        std::make_unique<FileService::FileSystemObjects>(directory);
  }
}

FileService::~FileService() {
  if (file_system_objects_)
    file_service_runner_->DeleteSoon(FROM_HERE, file_system_objects_.release());
  if (leveldb_objects_)
    leveldb_service_runner_->DeleteSoon(FROM_HERE, leveldb_objects_.release());
}

void FileService::BindFileSystem(
    mojo::PendingReceiver<mojom::FileSystem> receiver) {
  DCHECK(file_system_objects_)
      << "This FileService instance has no disk access";
  file_service_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&FileSystemObjects::BindFileSystemReceiver,
                     file_system_objects_->AsWeakPtr(), std::move(receiver)));
}

void FileService::BindLevelDBService(
    mojo::PendingReceiver<leveldb::mojom::LevelDBService> receiver) {
  if (!leveldb_objects_) {
    leveldb_objects_ = std::make_unique<FileService::LevelDBServiceObjects>(
        file_service_runner_);
  }

  if (leveldb_binder_override_) {
    leveldb_binder_override_.Run(std::move(receiver));
    return;
  }

  leveldb_service_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&LevelDBServiceObjects::BindLevelDBServiceReceiver,
                     leveldb_objects_->AsWeakPtr(), std::move(receiver)));
}

}  // namespace file
