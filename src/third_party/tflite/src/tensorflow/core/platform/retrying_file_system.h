/* Copyright 2016 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#ifndef TENSORFLOW_CORE_PLATFORM_CLOUD_RETRYING_FILE_SYSTEM_H_
#define TENSORFLOW_CORE_PLATFORM_CLOUD_RETRYING_FILE_SYSTEM_H_

#include <functional>
#include <string>
#include <vector>

#include "tensorflow/core/lib/random/random.h"
#include "tensorflow/core/platform/env.h"
#include "tensorflow/core/platform/errors.h"
#include "tensorflow/core/platform/file_system.h"
#include "tensorflow/core/platform/retrying_utils.h"
#include "tensorflow/core/platform/status.h"

namespace tensorflow {

/// A wrapper to add retry logic to another file system.
template <typename Underlying>
class RetryingFileSystem : public FileSystem {
 public:
  RetryingFileSystem(std::unique_ptr<Underlying> base_file_system,
                     const RetryConfig& retry_config)
      : base_file_system_(std::move(base_file_system)),
        retry_config_(retry_config) {}

  TF_USE_FILESYSTEM_METHODS_WITH_NO_TRANSACTION_SUPPORT;

  Status NewRandomAccessFile(
      const string& filename, TransactionToken* token,
      std::unique_ptr<RandomAccessFile>* result) override;

  Status NewWritableFile(const string& filename, TransactionToken* token,
                         std::unique_ptr<WritableFile>* result) override;

  Status NewAppendableFile(const string& filename, TransactionToken* token,
                           std::unique_ptr<WritableFile>* result) override;

  Status NewReadOnlyMemoryRegionFromFile(
      const string& filename, TransactionToken* token,
      std::unique_ptr<ReadOnlyMemoryRegion>* result) override;

  Status FileExists(const string& fname, TransactionToken* token) override {
    return RetryingUtils::CallWithRetries(
        [this, &fname, token]() {
          return base_file_system_->FileExists(fname, token);
        },
        retry_config_);
  }

  Status GetChildren(const string& dir, TransactionToken* token,
                     std::vector<string>* result) override {
    return RetryingUtils::CallWithRetries(
        [this, &dir, result, token]() {
          return base_file_system_->GetChildren(dir, token, result);
        },
        retry_config_);
  }

  Status GetMatchingPaths(const string& pattern, TransactionToken* token,
                          std::vector<string>* result) override {
    return RetryingUtils::CallWithRetries(
        [this, &pattern, result, token]() {
          return base_file_system_->GetMatchingPaths(pattern, token, result);
        },
        retry_config_);
  }

  Status Stat(const string& fname, TransactionToken* token,
              FileStatistics* stat) override {
    return RetryingUtils::CallWithRetries(
        [this, &fname, stat, token]() {
          return base_file_system_->Stat(fname, token, stat);
        },
        retry_config_);
  }

  Status DeleteFile(const string& fname, TransactionToken* token) override {
    return RetryingUtils::DeleteWithRetries(
        [this, &fname, token]() {
          return base_file_system_->DeleteFile(fname, token);
        },
        retry_config_);
  }

  Status CreateDir(const string& dirname, TransactionToken* token) override {
    return RetryingUtils::CallWithRetries(
        [this, &dirname, token]() {
          return base_file_system_->CreateDir(dirname, token);
        },
        retry_config_);
  }

  Status DeleteDir(const string& dirname, TransactionToken* token) override {
    return RetryingUtils::DeleteWithRetries(
        [this, &dirname, token]() {
          return base_file_system_->DeleteDir(dirname, token);
        },
        retry_config_);
  }

  Status GetFileSize(const string& fname, TransactionToken* token,
                     uint64* file_size) override {
    return RetryingUtils::CallWithRetries(
        [this, &fname, file_size, token]() {
          return base_file_system_->GetFileSize(fname, token, file_size);
        },
        retry_config_);
  }

  Status RenameFile(const string& src, const string& target,
                    TransactionToken* token) override {
    return RetryingUtils::CallWithRetries(
        [this, &src, &target, token]() {
          return base_file_system_->RenameFile(src, target, token);
        },
        retry_config_);
  }

  Status IsDirectory(const string& dirname, TransactionToken* token) override {
    return RetryingUtils::CallWithRetries(
        [this, &dirname, token]() {
          return base_file_system_->IsDirectory(dirname, token);
        },
        retry_config_);
  }

  Status HasAtomicMove(const string& path, bool* has_atomic_move) override {
    // this method does not need to be retried
    return base_file_system_->HasAtomicMove(path, has_atomic_move);
  }

  Status DeleteRecursively(const string& dirname, TransactionToken* token,
                           int64_t* undeleted_files,
                           int64_t* undeleted_dirs) override {
    return RetryingUtils::DeleteWithRetries(
        [this, &dirname, token, undeleted_files, undeleted_dirs]() {
          return base_file_system_->DeleteRecursively(
              dirname, token, undeleted_files, undeleted_dirs);
        },
        retry_config_);
  }

  void FlushCaches(TransactionToken* token) override {
    base_file_system_->FlushCaches(token);
  }

  Underlying* underlying() const { return base_file_system_.get(); }

 private:
  std::unique_ptr<Underlying> base_file_system_;
  const RetryConfig retry_config_;

  TF_DISALLOW_COPY_AND_ASSIGN(RetryingFileSystem);
};

namespace retrying_internals {

class RetryingRandomAccessFile : public RandomAccessFile {
 public:
  RetryingRandomAccessFile(std::unique_ptr<RandomAccessFile> base_file,
                           const RetryConfig& retry_config)
      : base_file_(std::move(base_file)), retry_config_(retry_config) {}

  Status Name(StringPiece* result) const override {
    return base_file_->Name(result);
  }

  Status Read(uint64 offset, size_t n, StringPiece* result,
              char* scratch) const override {
    return RetryingUtils::CallWithRetries(
        [this, offset, n, result, scratch]() {
          return base_file_->Read(offset, n, result, scratch);
        },
        retry_config_);
  }

 private:
  std::unique_ptr<RandomAccessFile> base_file_;
  const RetryConfig retry_config_;
};

class RetryingWritableFile : public WritableFile {
 public:
  RetryingWritableFile(std::unique_ptr<WritableFile> base_file,
                       const RetryConfig& retry_config)
      : base_file_(std::move(base_file)), retry_config_(retry_config) {}

  ~RetryingWritableFile() override {
    // Makes sure the retrying version of Close() is called in the destructor.
    Close().IgnoreError();
  }

  Status Append(StringPiece data) override {
    return RetryingUtils::CallWithRetries(
        [this, &data]() { return base_file_->Append(data); }, retry_config_);
  }
  Status Close() override {
    return RetryingUtils::CallWithRetries(
        [this]() { return base_file_->Close(); }, retry_config_);
  }
  Status Flush() override {
    return RetryingUtils::CallWithRetries(
        [this]() { return base_file_->Flush(); }, retry_config_);
  }
  Status Name(StringPiece* result) const override {
    return base_file_->Name(result);
  }
  Status Sync() override {
    return RetryingUtils::CallWithRetries(
        [this]() { return base_file_->Sync(); }, retry_config_);
  }
  Status Tell(int64_t* position) override {
    return RetryingUtils::CallWithRetries(
        [this, &position]() { return base_file_->Tell(position); },
        retry_config_);
  }

 private:
  std::unique_ptr<WritableFile> base_file_;
  const RetryConfig retry_config_;
};

}  // namespace retrying_internals

template <typename Underlying>
Status RetryingFileSystem<Underlying>::NewRandomAccessFile(
    const string& filename, TransactionToken* token,
    std::unique_ptr<RandomAccessFile>* result) {
  std::unique_ptr<RandomAccessFile> base_file;
  TF_RETURN_IF_ERROR(RetryingUtils::CallWithRetries(
      [this, &filename, &base_file, token]() {
        return base_file_system_->NewRandomAccessFile(filename, token,
                                                      &base_file);
      },
      retry_config_));
  result->reset(new retrying_internals::RetryingRandomAccessFile(
      std::move(base_file), retry_config_));
  return Status::OK();
}

template <typename Underlying>
Status RetryingFileSystem<Underlying>::NewWritableFile(
    const string& filename, TransactionToken* token,
    std::unique_ptr<WritableFile>* result) {
  std::unique_ptr<WritableFile> base_file;
  TF_RETURN_IF_ERROR(RetryingUtils::CallWithRetries(
      [this, &filename, &base_file, token]() {
        return base_file_system_->NewWritableFile(filename, token, &base_file);
      },
      retry_config_));
  result->reset(new retrying_internals::RetryingWritableFile(
      std::move(base_file), retry_config_));
  return Status::OK();
}

template <typename Underlying>
Status RetryingFileSystem<Underlying>::NewAppendableFile(
    const string& filename, TransactionToken* token,
    std::unique_ptr<WritableFile>* result) {
  std::unique_ptr<WritableFile> base_file;
  TF_RETURN_IF_ERROR(RetryingUtils::CallWithRetries(
      [this, &filename, &base_file, token]() {
        return base_file_system_->NewAppendableFile(filename, token,
                                                    &base_file);
      },
      retry_config_));
  result->reset(new retrying_internals::RetryingWritableFile(
      std::move(base_file), retry_config_));
  return Status::OK();
}

template <typename Underlying>
Status RetryingFileSystem<Underlying>::NewReadOnlyMemoryRegionFromFile(
    const string& filename, TransactionToken* token,
    std::unique_ptr<ReadOnlyMemoryRegion>* result) {
  return RetryingUtils::CallWithRetries(
      [this, &filename, result, token]() {
        return base_file_system_->NewReadOnlyMemoryRegionFromFile(
            filename, token, result);
      },
      retry_config_);
}

}  // namespace tensorflow

#endif  // TENSORFLOW_CORE_PLATFORM_CLOUD_RETRYING_FILE_SYSTEM_H_
