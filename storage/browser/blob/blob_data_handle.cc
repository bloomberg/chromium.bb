// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_data_handle.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner.h"
#include "base/time/time.h"
#include "storage/browser/blob/blob_data_snapshot.h"
#include "storage/browser/blob/blob_reader.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/fileapi/file_stream_reader.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "url/gurl.h"

namespace storage {

namespace {

class FileStreamReaderProviderImpl
    : public BlobReader::FileStreamReaderProvider {
 public:
  FileStreamReaderProviderImpl(FileSystemContext* file_system_context)
      : file_system_context_(file_system_context) {}
  ~FileStreamReaderProviderImpl() override {}

  scoped_ptr<FileStreamReader> CreateForLocalFile(
      base::TaskRunner* task_runner,
      const base::FilePath& file_path,
      int64_t initial_offset,
      const base::Time& expected_modification_time) override {
    return make_scoped_ptr(FileStreamReader::CreateForLocalFile(
        task_runner, file_path, initial_offset, expected_modification_time));
  }

  scoped_ptr<FileStreamReader> CreateFileStreamReader(
      const GURL& filesystem_url,
      int64_t offset,
      int64_t max_bytes_to_read,
      const base::Time& expected_modification_time) override {
    return file_system_context_->CreateFileStreamReader(
        storage::FileSystemURL(file_system_context_->CrackURL(filesystem_url)),
        offset, max_bytes_to_read, expected_modification_time);
  }

 private:
  scoped_refptr<FileSystemContext> file_system_context_;
  DISALLOW_COPY_AND_ASSIGN(FileStreamReaderProviderImpl);
};

}  // namespace

BlobDataHandle::BlobDataHandleShared::BlobDataHandleShared(
    const std::string& uuid,
    const std::string& content_type,
    const std::string& content_disposition,
    BlobStorageContext* context)
    : uuid_(uuid),
      content_type_(content_type),
      content_disposition_(content_disposition),
      context_(context->AsWeakPtr()) {
  context_->IncrementBlobRefCount(uuid);
}

scoped_ptr<BlobReader> BlobDataHandle::CreateReader(
    FileSystemContext* file_system_context,
    base::SequencedTaskRunner* file_task_runner) const {
  return scoped_ptr<BlobReader>(new BlobReader(
      this, scoped_ptr<BlobReader::FileStreamReaderProvider>(
                new FileStreamReaderProviderImpl(file_system_context)),
      file_task_runner));
}

scoped_ptr<BlobDataSnapshot>
BlobDataHandle::BlobDataHandleShared::CreateSnapshot() const {
  return context_->CreateSnapshot(uuid_);
}

BlobDataHandle::BlobDataHandleShared::~BlobDataHandleShared() {
  if (context_.get())
    context_->DecrementBlobRefCount(uuid_);
}

BlobDataHandle::BlobDataHandle(const std::string& uuid,
                               const std::string& content_type,
                               const std::string& content_disposition,
                               BlobStorageContext* context,
                               base::SequencedTaskRunner* io_task_runner)
    : io_task_runner_(io_task_runner),
      shared_(new BlobDataHandleShared(uuid,
                                       content_type,
                                       content_disposition,
                                       context)) {
  DCHECK(io_task_runner_.get());
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
}

BlobDataHandle::BlobDataHandle(const BlobDataHandle& other) {
  io_task_runner_ = other.io_task_runner_;
  shared_ = other.shared_;
}

BlobDataHandle::~BlobDataHandle() {
  BlobDataHandleShared* raw = shared_.get();
  raw->AddRef();
  shared_ = nullptr;
  io_task_runner_->ReleaseSoon(FROM_HERE, raw);
}

scoped_ptr<BlobDataSnapshot> BlobDataHandle::CreateSnapshot() const {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  return shared_->CreateSnapshot();
}

const std::string& BlobDataHandle::uuid() const {
  return shared_->uuid_;
}

const std::string& BlobDataHandle::content_type() const {
  return shared_->content_type_;
}

const std::string& BlobDataHandle::content_disposition() const {
  return shared_->content_disposition_;
}

}  // namespace storage
