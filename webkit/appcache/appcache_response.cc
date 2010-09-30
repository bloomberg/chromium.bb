// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache_response.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/pickle.h"
#include "base/string_util.h"
#include "net/base/net_errors.h"
#include "net/base/io_buffer.h"
#include "net/disk_cache/disk_cache.h"
#include "webkit/appcache/appcache_disk_cache.h"
#include "webkit/appcache/appcache_service.h"

using disk_cache::Entry;

namespace appcache {

namespace {

// Disk cache entry data indices.
enum {
  kResponseInfoIndex,
  kResponseContentIndex
};

// An IOBuffer that wraps a pickle's data. Ownership of the
// pickle is transfered to the WrappedPickleIOBuffer object.
class WrappedPickleIOBuffer : public net::WrappedIOBuffer {
 public:
  explicit WrappedPickleIOBuffer(const Pickle* pickle) :
      net::WrappedIOBuffer(reinterpret_cast<const char*>(pickle->data())),
      pickle_(pickle) {
    DCHECK(pickle->data());
  }

 private:
  ~WrappedPickleIOBuffer() {}

  scoped_ptr<const Pickle> pickle_;
};

}  // anon namespace


// AppCacheResponseInfo ----------------------------------------------

AppCacheResponseInfo::AppCacheResponseInfo(
    AppCacheService* service, const GURL& manifest_url,
    int64 response_id,  net::HttpResponseInfo* http_info,
    int64 response_data_size)
    : manifest_url_(manifest_url), response_id_(response_id),
      http_response_info_(http_info), response_data_size_(response_data_size),
      service_(service) {
  DCHECK(http_info);
  DCHECK(response_id != kNoResponseId);
  service_->storage()->working_set()->AddResponseInfo(this);
}

AppCacheResponseInfo::~AppCacheResponseInfo() {
  service_->storage()->working_set()->RemoveResponseInfo(this);
}

// HttpResponseInfoIOBuffer ------------------------------------------

HttpResponseInfoIOBuffer::HttpResponseInfoIOBuffer()
    : response_data_size(kUnkownResponseDataSize) {}

HttpResponseInfoIOBuffer::HttpResponseInfoIOBuffer(net::HttpResponseInfo* info)
    : http_info(info), response_data_size(kUnkownResponseDataSize) {}

HttpResponseInfoIOBuffer::~HttpResponseInfoIOBuffer() {}

// AppCacheResponseIO ----------------------------------------------

AppCacheResponseIO::AppCacheResponseIO(
    int64 response_id, AppCacheDiskCache* disk_cache)
    : response_id_(response_id), disk_cache_(disk_cache),
      entry_(NULL), buffer_len_(0), user_callback_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(raw_callback_(
          new net::CancelableCompletionCallback<AppCacheResponseIO>(
              this, &AppCacheResponseIO::OnRawIOComplete))) {
}

AppCacheResponseIO::~AppCacheResponseIO() {
  raw_callback_->Cancel();
  if (entry_)
    entry_->Close();
}

void AppCacheResponseIO::ScheduleIOCompletionCallback(int result) {
  MessageLoop::current()->PostTask(FROM_HERE,
      method_factory_.NewRunnableMethod(
          &AppCacheResponseIO::OnIOComplete, result));
}

void AppCacheResponseIO::InvokeUserCompletionCallback(int result) {
  // Clear the user callback and buffers prior to invoking the callback
  // so the caller can schedule additional operations in the callback.
  buffer_ = NULL;
  info_buffer_ = NULL;
  net::CompletionCallback* temp_user_callback = user_callback_;
  user_callback_ = NULL;
  temp_user_callback->Run(result);
}

void AppCacheResponseIO::ReadRaw(int index, int offset,
                                 net::IOBuffer* buf, int buf_len) {
  DCHECK(entry_);
  raw_callback_->AddRef();  // Balanced in OnRawIOComplete.
  int rv = entry_->ReadData(index, offset, buf, buf_len, raw_callback_);
  if (rv != net::ERR_IO_PENDING) {
    raw_callback_->Release();
    ScheduleIOCompletionCallback(rv);
  }
}

void AppCacheResponseIO::WriteRaw(int index, int offset,
                                 net::IOBuffer* buf, int buf_len) {
  DCHECK(entry_);
  const bool kTruncate = true;
  raw_callback_->AddRef();  // Balanced in OnRawIOComplete.
  int rv = entry_->WriteData(index, offset, buf, buf_len, raw_callback_,
                             kTruncate);
  if (rv != net::ERR_IO_PENDING) {
    raw_callback_->Release();
    ScheduleIOCompletionCallback(rv);
  }
}

void AppCacheResponseIO::OnRawIOComplete(int result) {
  raw_callback_->Release();  // Balance the AddRefs
  OnIOComplete(result);
}


// AppCacheResponseReader ----------------------------------------------

AppCacheResponseReader::AppCacheResponseReader(
    int64 response_id, AppCacheDiskCache* disk_cache)
    : AppCacheResponseIO(response_id, disk_cache),
      range_offset_(0), range_length_(kint32max),
      read_position_(0) {
}

AppCacheResponseReader::~AppCacheResponseReader() {
  if (open_callback_)
    open_callback_.release()->Cancel();
}

void AppCacheResponseReader::ReadInfo(HttpResponseInfoIOBuffer* info_buf,
                                      net::CompletionCallback* callback) {
  DCHECK(callback && !IsReadPending());
  DCHECK(info_buf && !info_buf->http_info.get());
  DCHECK(!buffer_.get() && !info_buffer_.get());

  info_buffer_ = info_buf;
  user_callback_ = callback;  // cleared on completion
  OpenEntryIfNeededAndContinue();
}

void AppCacheResponseReader::ContinueReadInfo() {
  if (!entry_)  {
    ScheduleIOCompletionCallback(net::ERR_CACHE_MISS);
    return;
  }

  int size = entry_->GetDataSize(kResponseInfoIndex);
  if (size <= 0) {
    ScheduleIOCompletionCallback(net::ERR_CACHE_MISS);
    return;
  }

  buffer_ = new net::IOBuffer(size);
  ReadRaw(kResponseInfoIndex, 0, buffer_.get(), size);
}

void AppCacheResponseReader::ReadData(net::IOBuffer* buf, int buf_len,
                                      net::CompletionCallback* callback) {
  DCHECK(callback && !IsReadPending());
  DCHECK(buf && (buf_len >= 0));
  DCHECK(!buffer_.get() && !info_buffer_.get());

  buffer_ = buf;
  buffer_len_ = buf_len;
  user_callback_ = callback;  // cleared on completion
  OpenEntryIfNeededAndContinue();
}

void AppCacheResponseReader::ContinueReadData() {
  if (!entry_)  {
    ScheduleIOCompletionCallback(net::ERR_CACHE_MISS);
    return;
  }

  if (read_position_ + buffer_len_ > range_length_) {
    // TODO(michaeln): What about integer overflows?
    DCHECK(range_length_ >= read_position_);
    buffer_len_ = range_length_ - read_position_;
  }
  ReadRaw(kResponseContentIndex, range_offset_ + read_position_,
          buffer_, buffer_len_);
}

void AppCacheResponseReader::SetReadRange(int offset, int length) {
  DCHECK(!IsReadPending() && !read_position_);
  range_offset_ = offset;
  range_length_ = length;
}

void AppCacheResponseReader::OnIOComplete(int result) {
  if (result >= 0) {
    if (info_buffer_.get()) {
      // Allocate and deserialize the http info structure.
      Pickle pickle(buffer_->data(), result);
      bool response_truncated = false;
      info_buffer_->http_info.reset(new net::HttpResponseInfo);
      info_buffer_->http_info->InitFromPickle(pickle, &response_truncated);
      DCHECK(!response_truncated);

      // Also return the size of the response body
      DCHECK(entry_);
      info_buffer_->response_data_size =
          entry_->GetDataSize(kResponseContentIndex);
    } else {
      read_position_ += result;
    }
  }
  InvokeUserCompletionCallback(result);
}

void AppCacheResponseReader::OpenEntryIfNeededAndContinue() {
  int rv;
  if (entry_) {
    rv = net::OK;
  } else if (!disk_cache_) {
    rv = net::ERR_FAILED;
  } else {
    open_callback_ = new EntryCallback<AppCacheResponseReader>(
        this, &AppCacheResponseReader::OnOpenEntryComplete);
    rv = disk_cache_->OpenEntry(response_id_, &open_callback_->entry_ptr_,
                                open_callback_.get());
  }

  if (rv != net::ERR_IO_PENDING)
    OnOpenEntryComplete(rv);
}

void AppCacheResponseReader::OnOpenEntryComplete(int rv) {
  DCHECK(info_buffer_.get() || buffer_.get());

  if (open_callback_) {
    if (rv == net::OK) {
      entry_ = open_callback_->entry_ptr_;
      open_callback_->entry_ptr_ = NULL;
    }
    open_callback_ = NULL;
  }

  if (info_buffer_)
    ContinueReadInfo();
  else
    ContinueReadData();
}

// AppCacheResponseWriter ----------------------------------------------

AppCacheResponseWriter::AppCacheResponseWriter(
    int64 response_id, AppCacheDiskCache* disk_cache)
    : AppCacheResponseIO(response_id, disk_cache),
      info_size_(0), write_position_(0), write_amount_(0),
      creation_phase_(INITIAL_ATTEMPT) {
}

AppCacheResponseWriter::~AppCacheResponseWriter() {
  if (create_callback_)
    create_callback_.release()->Cancel();
}

void AppCacheResponseWriter::WriteInfo(HttpResponseInfoIOBuffer* info_buf,
                                       net::CompletionCallback* callback) {
  DCHECK(callback && !IsWritePending());
  DCHECK(info_buf && info_buf->http_info.get());
  DCHECK(!buffer_.get() && !info_buffer_.get());

  info_buffer_ = info_buf;
  user_callback_ = callback;  // cleared on completion
  CreateEntryIfNeededAndContinue();
}

void AppCacheResponseWriter::ContinueWriteInfo() {
  if (!entry_) {
    ScheduleIOCompletionCallback(net::ERR_FAILED);
    return;
  }

  const bool kSkipTransientHeaders = true;
  const bool kTruncated = false;
  Pickle* pickle = new Pickle;
  info_buffer_->http_info->Persist(pickle, kSkipTransientHeaders, kTruncated);
  write_amount_ = static_cast<int>(pickle->size());
  buffer_ = new WrappedPickleIOBuffer(pickle);  // takes ownership of pickle
  WriteRaw(kResponseInfoIndex, 0, buffer_, write_amount_);
}

void AppCacheResponseWriter::WriteData(net::IOBuffer* buf, int buf_len,
                                       net::CompletionCallback* callback) {
  DCHECK(callback && !IsWritePending());
  DCHECK(buf && (buf_len >= 0));
  DCHECK(!buffer_.get() && !info_buffer_.get());

  buffer_ = buf;
  write_amount_ = buf_len;
  user_callback_ = callback;  // cleared on completion
  CreateEntryIfNeededAndContinue();
}

void AppCacheResponseWriter::ContinueWriteData() {
  if (!entry_) {
    ScheduleIOCompletionCallback(net::ERR_FAILED);
    return;
  }
  WriteRaw(kResponseContentIndex, write_position_, buffer_, write_amount_);
}

void AppCacheResponseWriter::OnIOComplete(int result) {
  if (result >= 0) {
    DCHECK(write_amount_ == result);
    if (!info_buffer_.get())
      write_position_ += result;
    else
      info_size_ = result;
  }
  InvokeUserCompletionCallback(result);
}

void AppCacheResponseWriter::CreateEntryIfNeededAndContinue() {
  int rv;
  if (entry_) {
    creation_phase_ = NO_ATTEMPT;
    rv = net::OK;
  } else if (!disk_cache_) {
    creation_phase_ = NO_ATTEMPT;
    rv = net::ERR_FAILED;
  } else {
    creation_phase_ = INITIAL_ATTEMPT;
    create_callback_ = new EntryCallback<AppCacheResponseWriter>(
        this, &AppCacheResponseWriter::OnCreateEntryComplete);
    rv = disk_cache_->CreateEntry(response_id_, &create_callback_->entry_ptr_,
                                  create_callback_.get());
  }
  if (rv != net::ERR_IO_PENDING)
    OnCreateEntryComplete(rv);
}

void AppCacheResponseWriter::OnCreateEntryComplete(int rv) {
  DCHECK(info_buffer_.get() || buffer_.get());

  if (creation_phase_ == INITIAL_ATTEMPT) {
    if (rv != net::OK) {
      // We may try to overrite existing entries.
      creation_phase_ = DOOM_EXISTING;
      rv = disk_cache_->DoomEntry(response_id_, create_callback_.get());
      if (rv != net::ERR_IO_PENDING)
        OnCreateEntryComplete(rv);
      return;
    }
  } else if (creation_phase_ == DOOM_EXISTING) {
    creation_phase_ = SECOND_ATTEMPT;
    rv = disk_cache_->CreateEntry(response_id_, &create_callback_->entry_ptr_,
                                  create_callback_.get());
    if (rv != net::ERR_IO_PENDING)
      OnCreateEntryComplete(rv);
    return;
  }

  if (create_callback_) {
    if (rv == net::OK) {
      entry_ = create_callback_->entry_ptr_;
      create_callback_->entry_ptr_ = NULL;
    }
    create_callback_ = NULL;
  }

  if (info_buffer_)
    ContinueWriteInfo();
  else
    ContinueWriteData();
}

}  // namespace appcache

