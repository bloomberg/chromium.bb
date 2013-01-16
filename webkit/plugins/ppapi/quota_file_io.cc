// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/quota_file_io.h"

#include <algorithm>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/stl_util.h"
#include "base/task_runner_util.h"
#include "webkit/plugins/ppapi/host_globals.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/resource_helper.h"

using base::PlatformFile;
using base::PlatformFileError;
using quota::StorageType;

namespace webkit {
namespace ppapi {

namespace {
StorageType PPFileSystemTypeToQuotaStorageType(PP_FileSystemType type) {
  switch (type) {
    case PP_FILESYSTEMTYPE_LOCALPERSISTENT:
      return quota::kStorageTypePersistent;
    case PP_FILESYSTEMTYPE_LOCALTEMPORARY:
      return quota::kStorageTypeTemporary;
    default:
      return quota::kStorageTypeUnknown;
  }
  NOTREACHED();
  return quota::kStorageTypeUnknown;
}

int WriteAdapter(PlatformFile file, int64 offset,
                 scoped_ptr<char[]> data, int size) {
  return base::WritePlatformFile(file, offset, data.get(), size);
}

}  // namespace

class QuotaFileIO::PendingOperationBase {
 public:
  virtual ~PendingOperationBase() {}

  // Either one of Run() or DidFail() is called (the latter is called when
  // there was more than one error during quota queries).
  virtual void Run() = 0;
  virtual void DidFail(PlatformFileError error) = 0;

 protected:
  PendingOperationBase(QuotaFileIO* quota_io, bool is_will_operation)
      : quota_io_(quota_io), is_will_operation_(is_will_operation) {
    DCHECK(quota_io_);
    quota_io_->WillUpdate();
  }

  QuotaFileIO* quota_io_;
  const bool is_will_operation_;
};

class QuotaFileIO::WriteOperation : public PendingOperationBase {
 public:
  WriteOperation(QuotaFileIO* quota_io,
                 bool is_will_operation,
                 int64_t offset,
                 const char* buffer,
                 int32_t bytes_to_write,
                 const WriteCallback& callback)
      : PendingOperationBase(quota_io, is_will_operation),
        offset_(offset),
        bytes_to_write_(bytes_to_write),
        callback_(callback),
        finished_(false),
        status_(base::PLATFORM_FILE_OK),
        bytes_written_(0),
        ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
    if (!is_will_operation) {
      // TODO(kinuko): Check the API convention if we really need to keep a copy
      // of the buffer during the async write operations.
      buffer_.reset(new char[bytes_to_write]);
      memcpy(buffer_.get(), buffer, bytes_to_write);
    }
  }
  virtual ~WriteOperation() {}
  virtual void Run() OVERRIDE {
    DCHECK(quota_io_);
    if (quota_io_->CheckIfExceedsQuota(offset_ + bytes_to_write_)) {
      DidFail(base::PLATFORM_FILE_ERROR_NO_SPACE);
      return;
    }
    if (is_will_operation_) {
      // Assuming the write will succeed.
      DidFinish(base::PLATFORM_FILE_OK, bytes_to_write_);
      return;
    }
    DCHECK(buffer_.get());

    PluginDelegate* plugin_delegate = quota_io_->GetPluginDelegate();
    if (!plugin_delegate) {
      DidFail(base::PLATFORM_FILE_ERROR_FAILED);
      return;
    }

    if (!base::PostTaskAndReplyWithResult(
            plugin_delegate->GetFileThreadMessageLoopProxy(), FROM_HERE,
            base::Bind(&WriteAdapter,
                       quota_io_->file_, offset_,
                       base::Passed(&buffer_), bytes_to_write_),
            base::Bind(&WriteOperation::DidWrite,
                       weak_factory_.GetWeakPtr()))) {
      DidFail(base::PLATFORM_FILE_ERROR_FAILED);
      return;
    }
  }

  virtual void DidFail(PlatformFileError error) OVERRIDE {
    DidFinish(error, 0);
  }

  bool finished() const { return finished_; }

  virtual void WillRunCallback() {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(&WriteOperation::RunCallback, weak_factory_.GetWeakPtr()));
  }

 private:
  void DidWrite(int bytes_written) {
    base::PlatformFileError error = bytes_written > 0 ?
        base::PLATFORM_FILE_OK : base::PLATFORM_FILE_ERROR_FAILED;
    DidFinish(error, bytes_written);
  }

  void DidFinish(PlatformFileError status, int bytes_written) {
    finished_ = true;
    status_ = status;
    bytes_written_ = bytes_written;
    int64_t max_offset =
        (status != base::PLATFORM_FILE_OK) ? 0 : offset_ + bytes_written;
    // This may delete itself by calling RunCallback.
    quota_io_->DidWrite(this, max_offset);
  }

  virtual void RunCallback() {
    DCHECK_EQ(false, callback_.is_null());
    callback_.Run(status_, bytes_written_);
    delete this;
  }

  const int64_t offset_;
  scoped_ptr<char[]> buffer_;
  const int32_t bytes_to_write_;
  WriteCallback callback_;
  bool finished_;
  PlatformFileError status_;
  int64_t bytes_written_;
  base::WeakPtrFactory<WriteOperation> weak_factory_;
};

class QuotaFileIO::SetLengthOperation : public PendingOperationBase {
 public:
  SetLengthOperation(QuotaFileIO* quota_io,
                     bool is_will_operation,
                     int64_t length,
                     const StatusCallback& callback)
      : PendingOperationBase(quota_io, is_will_operation),
        length_(length),
        callback_(callback),
        ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {}

  virtual ~SetLengthOperation() {}

  virtual void Run() OVERRIDE {
    DCHECK(quota_io_);
    if (quota_io_->CheckIfExceedsQuota(length_)) {
      DidFail(base::PLATFORM_FILE_ERROR_NO_SPACE);
      return;
    }
    if (is_will_operation_) {
      DidFinish(base::PLATFORM_FILE_OK);
      return;
    }

    PluginDelegate* plugin_delegate = quota_io_->GetPluginDelegate();
    if (!plugin_delegate) {
      DidFail(base::PLATFORM_FILE_ERROR_FAILED);
      return;
    }

    if (!base::FileUtilProxy::Truncate(
            plugin_delegate->GetFileThreadMessageLoopProxy(),
            quota_io_->file_, length_,
            base::Bind(&SetLengthOperation::DidFinish,
                       weak_factory_.GetWeakPtr()))) {
      DidFail(base::PLATFORM_FILE_ERROR_FAILED);
      return;
    }
  }

  virtual void DidFail(PlatformFileError error) OVERRIDE {
    DidFinish(error);
  }

 private:
  void DidFinish(PlatformFileError status) {
    quota_io_->DidSetLength(status, length_);
    DCHECK_EQ(false, callback_.is_null());
    callback_.Run(status);
    delete this;
  }

  int64_t length_;
  StatusCallback callback_;
  base::WeakPtrFactory<SetLengthOperation> weak_factory_;
};

// QuotaFileIO --------------------------------------------------------------

QuotaFileIO::QuotaFileIO(
    PP_Instance instance,
    PlatformFile file,
    const GURL& file_url,
    PP_FileSystemType type)
    : pp_instance_(instance),
      file_(file),
      file_url_(file_url),
      storage_type_(PPFileSystemTypeToQuotaStorageType(type)),
      cached_file_size_(0),
      cached_available_space_(0),
      outstanding_quota_queries_(0),
      outstanding_errors_(0),
      max_written_offset_(0),
      inflight_operations_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  DCHECK_NE(base::kInvalidPlatformFileValue, file_);
  DCHECK_NE(quota::kStorageTypeUnknown, storage_type_);
}

QuotaFileIO::~QuotaFileIO() {
  // Note that this doesn't dispatch pending callbacks.
  STLDeleteContainerPointers(pending_operations_.begin(),
                             pending_operations_.end());
  STLDeleteContainerPointers(pending_callbacks_.begin(),
                             pending_callbacks_.end());
}

bool QuotaFileIO::Write(
    int64_t offset, const char* buffer, int32_t bytes_to_write,
    const WriteCallback& callback) {
  if (bytes_to_write <= 0)
    return false;

  WriteOperation* op = new WriteOperation(
      this, false, offset, buffer, bytes_to_write, callback);
  return RegisterOperationForQuotaChecks(op);
}

bool QuotaFileIO::SetLength(int64_t length, const StatusCallback& callback) {
  DCHECK(pending_operations_.empty());
  SetLengthOperation* op = new SetLengthOperation(
      this, false, length, callback);
  return RegisterOperationForQuotaChecks(op);
}

bool QuotaFileIO::WillWrite(
    int64_t offset, int32_t bytes_to_write, const WriteCallback& callback) {
  WriteOperation* op = new WriteOperation(
      this, true, offset, NULL, bytes_to_write, callback);
  return RegisterOperationForQuotaChecks(op);
}

bool QuotaFileIO::WillSetLength(int64_t length,
                                const StatusCallback& callback) {
  DCHECK(pending_operations_.empty());
  SetLengthOperation* op = new SetLengthOperation(this, true, length, callback);
  return RegisterOperationForQuotaChecks(op);
}

PluginDelegate* QuotaFileIO::GetPluginDelegate() const {
  PluginInstance* instance = HostGlobals::Get()->GetInstance(pp_instance_);
  if (instance)
    return instance->delegate();
  return NULL;
}

bool QuotaFileIO::RegisterOperationForQuotaChecks(
    PendingOperationBase* op_ptr) {
  scoped_ptr<PendingOperationBase> op(op_ptr);
  if (pending_operations_.empty()) {
    // This is the first pending quota check. Run querying the file size
    // and available space.
    outstanding_quota_queries_ = 0;
    outstanding_errors_ = 0;

    PluginDelegate* plugin_delegate = GetPluginDelegate();
    if (!plugin_delegate)
      return false;

    // Query the file size.
    ++outstanding_quota_queries_;
    if (!base::FileUtilProxy::GetFileInfoFromPlatformFile(
            plugin_delegate->GetFileThreadMessageLoopProxy(), file_,
            base::Bind(&QuotaFileIO::DidQueryInfoForQuota,
                       weak_factory_.GetWeakPtr()))) {
      // This makes the call fail synchronously; we do not fire the callback
      // here but just delete the operation and return false.
      return false;
    }

    // Query the current available space.
    ++outstanding_quota_queries_;
    plugin_delegate->QueryAvailableSpace(
        file_url_.GetOrigin(), storage_type_,
        base::Bind(&QuotaFileIO::DidQueryAvailableSpace,
                   weak_factory_.GetWeakPtr()));
  }
  pending_operations_.push_back(op.release());
  return true;
}

void QuotaFileIO::DidQueryInfoForQuota(
    base::PlatformFileError error_code,
    const base::PlatformFileInfo& file_info) {
  if (error_code != base::PLATFORM_FILE_OK)
    ++outstanding_errors_;
  cached_file_size_ = file_info.size;
  DCHECK_GT(outstanding_quota_queries_, 0);
  if (--outstanding_quota_queries_ == 0)
    DidQueryForQuotaCheck();
}

void QuotaFileIO::DidQueryAvailableSpace(int64_t avail_space) {
  cached_available_space_ = avail_space;
  DCHECK_GT(outstanding_quota_queries_, 0);
  if (--outstanding_quota_queries_ == 0)
    DidQueryForQuotaCheck();
}

void QuotaFileIO::DidQueryForQuotaCheck() {
  DCHECK(!pending_operations_.empty());
  DCHECK_GT(inflight_operations_, 0);
  while (!pending_operations_.empty()) {
    PendingOperationBase* op = pending_operations_.front();
    pending_operations_.pop_front();
    pending_callbacks_.push_back(op);
    if (outstanding_errors_ > 0) {
      op->DidFail(base::PLATFORM_FILE_ERROR_FAILED);
      continue;
    }
    op->Run();
  }
}

bool QuotaFileIO::CheckIfExceedsQuota(int64_t new_file_size) const {
  DCHECK_GE(cached_file_size_, 0);
  DCHECK_GE(cached_available_space_, 0);
  return new_file_size - cached_file_size_ > cached_available_space_;
}

void QuotaFileIO::WillUpdate() {
  if (inflight_operations_++ == 0) {
    PluginDelegate* plugin_delegate = GetPluginDelegate();
    if (plugin_delegate)
      plugin_delegate->WillUpdateFile(file_url_);
    DCHECK_EQ(0, max_written_offset_);
  }
}

void QuotaFileIO::DidWrite(WriteOperation* op,
                           int64_t written_offset_end) {
  max_written_offset_ = std::max(max_written_offset_, written_offset_end);
  DCHECK_GT(inflight_operations_, 0);
  DCHECK(!pending_callbacks_.empty());
  // Fire callbacks for finished operations.
  while (!pending_callbacks_.empty()) {
    WriteOperation* op = static_cast<WriteOperation*>(
        pending_callbacks_.front());
    if (!op->finished())
      break;
    pending_callbacks_.pop_front();
    op->WillRunCallback();
  }
  // If we have no more pending writes, notify the browser that we did
  // update the file.
  if (--inflight_operations_ == 0) {
    DCHECK(pending_operations_.empty());
    int64_t growth = max_written_offset_ - cached_file_size_;
    growth = growth < 0 ? 0 : growth;

    PluginDelegate* plugin_delegate = GetPluginDelegate();
    if (plugin_delegate)
      plugin_delegate->DidUpdateFile(file_url_, growth);
    max_written_offset_ = 0;
  }
}

void QuotaFileIO::DidSetLength(PlatformFileError error, int64_t new_file_size) {
  DCHECK_EQ(1, inflight_operations_);
  pending_callbacks_.pop_front();
  DCHECK(pending_callbacks_.empty());
  int64_t delta = (error != base::PLATFORM_FILE_OK) ? 0 :
      new_file_size - cached_file_size_;


  PluginDelegate* plugin_delegate = GetPluginDelegate();
  if (plugin_delegate)
    plugin_delegate->DidUpdateFile(file_url_, delta);
  inflight_operations_ = 0;
}

}  // namespace ppapi
}  // namespace webkit
