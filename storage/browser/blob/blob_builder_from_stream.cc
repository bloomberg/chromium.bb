// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_builder_from_stream.h"

#include "base/containers/span.h"
#include "base/guid.h"
#include "base/task_scheduler/post_task.h"
#include "storage/browser/blob/blob_data_item.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/shareable_file_reference.h"

namespace storage {

namespace {

// Size of individual type kBytes items the blob will be build from. The real
// limit is the min of this and limits_.max_bytes_data_item_size.
constexpr size_t kMaxMemoryChunkSize = 512 * 1024;

// Helper base-class that reads upto a certain number of bytes from a data pipe.
// Deletes itself when done.
class DataPipeConsumerHelper {
 protected:
  DataPipeConsumerHelper(mojo::ScopedDataPipeConsumerHandle pipe,
                         uint64_t max_bytes_to_read)
      : pipe_(std::move(pipe)),
        watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
        max_bytes_to_read_(max_bytes_to_read) {
    watcher_.Watch(pipe_.get(), MOJO_HANDLE_SIGNAL_READABLE,
                   MOJO_WATCH_CONDITION_SATISFIED,
                   base::BindRepeating(&DataPipeConsumerHelper::DataPipeReady,
                                       base::Unretained(this)));
    watcher_.ArmOrNotify();
  }
  virtual ~DataPipeConsumerHelper() = default;

  virtual void Populate(base::span<const char> data, uint64_t offset) = 0;
  virtual void InvokeDone(mojo::ScopedDataPipeConsumerHandle pipe,
                          uint64_t bytes_written) = 0;

 private:
  void DataPipeReady(MojoResult result, const mojo::HandleSignalsState& state) {
    while (current_offset_ < max_bytes_to_read_) {
      const void* data;
      uint32_t size;
      MojoResult result =
          pipe_->BeginReadData(&data, &size, MOJO_READ_DATA_FLAG_NONE);
      if (result == MOJO_RESULT_SHOULD_WAIT) {
        watcher_.ArmOrNotify();
        return;
      }

      if (result == MOJO_RESULT_FAILED_PRECONDITION) {
        // Pipe has closed, so we must be done.
        pipe_.reset();
        break;
      }
      DCHECK_EQ(MOJO_RESULT_OK, result);
      size = std::min<uint64_t>(size, max_bytes_to_read_ - current_offset_);
      Populate(base::make_span(static_cast<const char*>(data), size),
               current_offset_);
      current_offset_ += size;
      result = pipe_->EndReadData(size);
      DCHECK_EQ(MOJO_RESULT_OK, result);
    }

    // Either the pipe closed, or we filled the entire item.
    InvokeDone(std::move(pipe_), current_offset_);
    delete this;
  }

  mojo::ScopedDataPipeConsumerHandle pipe_;
  mojo::SimpleWatcher watcher_;
  const uint64_t max_bytes_to_read_;
  uint64_t current_offset_ = 0;
};

}  // namespace

// Helper class that reads upto a certain number of bytes from a datapipe and
// writes those bytes to a file. When done, or if the pipe is closed, calls its
// callback.
class BlobBuilderFromStream::WritePipeToFileHelper
    : public DataPipeConsumerHelper {
 public:
  using DoneCallback =
      base::OnceCallback<void(uint64_t bytes_written,
                              mojo::ScopedDataPipeConsumerHandle pipe,
                              const base::Time& modification_time)>;

  static void CreateAndStart(mojo::ScopedDataPipeConsumerHandle pipe,
                             base::File file,
                             uint64_t max_file_size,
                             DoneCallback callback) {
    base::CreateSequencedTaskRunnerWithTraits({base::MayBlock()})
        ->PostTask(
            FROM_HERE,
            base::BindOnce(&WritePipeToFileHelper::CreateAndStartOnFileSequence,
                           std::move(pipe), std::move(file), max_file_size,
                           base::SequencedTaskRunnerHandle::Get(),
                           std::move(callback)));
  }

 private:
  static void CreateAndStartOnFileSequence(
      mojo::ScopedDataPipeConsumerHandle pipe,
      base::File file,
      uint64_t max_file_size,
      scoped_refptr<base::TaskRunner> reply_runner,
      DoneCallback callback) {
    new WritePipeToFileHelper(std::move(pipe), std::move(file), max_file_size,
                              std::move(reply_runner), std::move(callback));
  }

  WritePipeToFileHelper(mojo::ScopedDataPipeConsumerHandle pipe,
                        base::File file,
                        uint64_t max_file_size,
                        scoped_refptr<base::TaskRunner> reply_runner,
                        DoneCallback callback)
      : DataPipeConsumerHelper(std::move(pipe), max_file_size),
        file_(std::move(file)),
        reply_runner_(std::move(reply_runner)),
        callback_(std::move(callback)) {}

  void Populate(base::span<const char> data, uint64_t offset) override {
    file_.Write(offset, data.data(), data.size());
  }

  void InvokeDone(mojo::ScopedDataPipeConsumerHandle pipe,
                  uint64_t bytes_written) override {
    base::Time last_modified;
    base::File::Info info;
    if (file_.Flush() && file_.GetInfo(&info))
      last_modified = info.last_modified;
    reply_runner_->PostTask(FROM_HERE,
                            base::BindOnce(std::move(callback_), bytes_written,
                                           std::move(pipe), last_modified));
  }

  base::File file_;
  scoped_refptr<base::TaskRunner> reply_runner_;
  DoneCallback callback_;
};

// Similar helper class that writes upto a certain number of bytes from a data
// pipe into a FutureData element.
class BlobBuilderFromStream::WritePipeToFutureDataHelper
    : public DataPipeConsumerHelper {
 public:
  using DoneCallback =
      base::OnceCallback<void(uint64_t bytes_written,
                              mojo::ScopedDataPipeConsumerHandle pipe)>;

  static void CreateAndStart(mojo::ScopedDataPipeConsumerHandle pipe,
                             scoped_refptr<BlobDataItem> item,
                             DoneCallback callback) {
    new WritePipeToFutureDataHelper(std::move(pipe), std::move(item),
                                    std::move(callback));
  }

 private:
  WritePipeToFutureDataHelper(mojo::ScopedDataPipeConsumerHandle pipe,
                              scoped_refptr<BlobDataItem> item,
                              DoneCallback callback)
      : DataPipeConsumerHelper(std::move(pipe), item->length()),
        item_(std::move(item)),
        callback_(std::move(callback)) {}

  void Populate(base::span<const char> data, uint64_t offset) override {
    if (item_->type() == BlobDataItem::Type::kBytesDescription)
      item_->AllocateBytes();
    std::memcpy(item_->mutable_bytes().subspan(offset, data.length()).data(),
                data.data(), data.length());
  }

  void InvokeDone(mojo::ScopedDataPipeConsumerHandle pipe,
                  uint64_t bytes_written) override {
    std::move(callback_).Run(bytes_written, std::move(pipe));
  }

  scoped_refptr<BlobDataItem> item_;
  DoneCallback callback_;
};

BlobBuilderFromStream::BlobBuilderFromStream(
    base::WeakPtr<BlobStorageContext> context,
    std::string content_type,
    std::string content_disposition,
    uint64_t length_hint,
    mojo::ScopedDataPipeConsumerHandle data,
    ResultCallback callback)
    : kMemoryBlockSize(std::min(
          kMaxMemoryChunkSize,
          context->memory_controller().limits().max_bytes_data_item_size)),
      kMaxBytesInMemory(
          context->memory_controller().limits().min_page_file_size),
      kFileBlockSize(context->memory_controller().limits().min_page_file_size),
      context_(std::move(context)),
      callback_(std::move(callback)),
      content_type_(std::move(content_type)),
      content_disposition_(std::move(content_disposition)),
      weak_factory_(this) {
  DCHECK(context_);

  // TODO(mek): Take length_hint into account to determine strategy.
  AllocateMoreSpace(std::move(data));
}

BlobBuilderFromStream::~BlobBuilderFromStream() {
  OnError();
}

void BlobBuilderFromStream::AllocateMoreSpace(
    mojo::ScopedDataPipeConsumerHandle pipe) {
  if (!context_ || !callback_) {
    OnError();
    return;
  }

  // If too much data has already been saved in memory, switch to using disk
  // backed data.
  if (ShouldStoreNextBlockOnDisk()) {
    AllocateMoreFileSpace(std::move(pipe));
    return;
  }

  if (context_->memory_controller().GetAvailableMemoryForBlobs() <
      kMemoryBlockSize) {
    OnError();
    return;
  }
  auto item = base::MakeRefCounted<ShareableBlobDataItem>(
      BlobDataItem::CreateBytesDescription(kMemoryBlockSize),
      ShareableBlobDataItem::QUOTA_NEEDED);
  pending_quota_task_ =
      context_->mutable_memory_controller()->ReserveMemoryQuota(
          {item},
          base::BindOnce(&BlobBuilderFromStream::QuotaAllocated,
                         base::Unretained(this), std::move(pipe), item));
}

void BlobBuilderFromStream::QuotaAllocated(
    mojo::ScopedDataPipeConsumerHandle pipe,
    scoped_refptr<ShareableBlobDataItem> item,
    bool success) {
  if (!success || !context_ || !callback_) {
    OnError();
    return;
  }
  WritePipeToFutureDataHelper::CreateAndStart(
      std::move(pipe), item->item(),
      base::BindOnce(&BlobBuilderFromStream::DidWriteToMemory,
                     weak_factory_.GetWeakPtr(), item));
}

void BlobBuilderFromStream::DidWriteToMemory(
    scoped_refptr<ShareableBlobDataItem> item,
    uint64_t bytes_written,
    mojo::ScopedDataPipeConsumerHandle pipe) {
  if (!context_ || !callback_) {
    OnError();
    return;
  }
  item->set_state(ShareableBlobDataItem::POPULATED_WITH_QUOTA);
  current_total_size_ += bytes_written;
  if (pipe.is_valid()) {
    DCHECK_EQ(item->item()->length(), bytes_written);
    items_.push_back(std::move(item));
    AllocateMoreSpace(std::move(pipe));
  } else {
    // Pipe has closed, so we must be done.
    DCHECK_LE(bytes_written, item->item()->length());
    if (bytes_written > 0) {
      item->item()->ShrinkBytes(bytes_written);
      context_->mutable_memory_controller()->ShrinkMemoryAllocation(item.get());
      items_.push_back(std::move(item));
    }
    OnSuccess();
  }
}

void BlobBuilderFromStream::AllocateMoreFileSpace(
    mojo::ScopedDataPipeConsumerHandle pipe) {
  if (!context_ || !callback_) {
    OnError();
    return;
  }

  // TODO(mek): Extend existing file until max_page_file_size is reached, rather
  // then creating multiple min_page_file_size files.

  if (context_->memory_controller().GetAvailableFileSpaceForBlobs() <
      kFileBlockSize) {
    OnError();
    return;
  }
  auto item = base::MakeRefCounted<ShareableBlobDataItem>(
      BlobDataItem::CreateFutureFile(0, kFileBlockSize, 0),
      ShareableBlobDataItem::QUOTA_NEEDED);
  pending_quota_task_ = context_->mutable_memory_controller()->ReserveFileQuota(
      {item},
      base::BindOnce(&BlobBuilderFromStream::FileQuotaAllocated,
                     weak_factory_.GetWeakPtr(), std::move(pipe), item));
}

void BlobBuilderFromStream::FileQuotaAllocated(
    mojo::ScopedDataPipeConsumerHandle pipe,
    scoped_refptr<ShareableBlobDataItem> item,
    std::vector<BlobMemoryController::FileCreationInfo> info,
    bool success) {
  if (!success || !context_ || !callback_) {
    OnError();
    return;
  }
  DCHECK_EQ(1u, info.size());
  WritePipeToFileHelper::CreateAndStart(
      std::move(pipe), std::move(info[0].file), item->item()->length(),
      base::BindOnce(&BlobBuilderFromStream::DidWriteToFile,
                     weak_factory_.GetWeakPtr(), item, info[0].file_reference));
}

void BlobBuilderFromStream::DidWriteToFile(
    scoped_refptr<ShareableBlobDataItem> item,
    scoped_refptr<ShareableFileReference> file,
    uint64_t bytes_written,
    mojo::ScopedDataPipeConsumerHandle pipe,
    const base::Time& modification_time) {
  if (!context_ || !callback_) {
    OnError();
    return;
  }
  item->item()->PopulateFile(file->path(), modification_time, file);
  item->set_state(ShareableBlobDataItem::POPULATED_WITH_QUOTA);
  current_total_size_ += bytes_written;
  if (pipe.is_valid()) {
    DCHECK_EQ(item->item()->length(), bytes_written);
    items_.push_back(std::move(item));
    // Once we start writing to file, we keep writing to file.
    AllocateMoreFileSpace(std::move(pipe));
  } else {
    // Pipe has closed, so we must be done.
    DCHECK_LE(bytes_written, item->item()->length());
    if (bytes_written > 0) {
      context_->mutable_memory_controller()->ShrinkFileAllocation(
          file.get(), item->item()->length(), bytes_written);
      item->item()->ShrinkFile(bytes_written);
      items_.push_back(std::move(item));
    }
    OnSuccess();
  }
}

void BlobBuilderFromStream::OnError() {
  if (pending_quota_task_)
    pending_quota_task_->Cancel();

  // Clear |items_| to avoid holding on to ShareableDataItems.
  items_.clear();

  if (!callback_)
    return;
  std::move(callback_).Run(this, nullptr);
}

void BlobBuilderFromStream::OnSuccess() {
  DCHECK(context_);
  DCHECK(callback_);
  std::move(callback_).Run(
      this, context_->AddFinishedBlob(base::GenerateGUID(), content_type_,
                                      content_disposition_, std::move(items_)));
}

bool BlobBuilderFromStream::ShouldStoreNextBlockOnDisk() {
  DCHECK(context_);
  const BlobMemoryController& controller = context_->memory_controller();
  if (!controller.file_paging_enabled())
    return false;
  if (current_total_size_ + kMemoryBlockSize > kMaxBytesInMemory &&
      controller.GetAvailableFileSpaceForBlobs() >= kFileBlockSize) {
    return true;
  }
  return controller.GetAvailableMemoryForBlobs() < kMemoryBlockSize;
}

}  // namespace storage
