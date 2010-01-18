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
#include "webkit/appcache/appcache_service.h"

using disk_cache::Entry;

namespace appcache {

namespace {

// Disk cache entry data indices.
enum {
  kResponseInfoIndex,
  kResponseContentIndex
};

// Disk cache entry keys.
std::string response_key(int64 response_id) {
  return Int64ToString(response_id);
}

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
    int64 response_id,  net::HttpResponseInfo* http_info)
    : manifest_url_(manifest_url), response_id_(response_id),
      http_response_info_(http_info), service_(service) {
  DCHECK(http_info);
  DCHECK(response_id != kNoResponseId);
  service_->storage()->working_set()->AddResponseInfo(this);
}

AppCacheResponseInfo::~AppCacheResponseInfo() {
  service_->storage()->working_set()->RemoveResponseInfo(this);
}


// AppCacheResponseIO ----------------------------------------------

AppCacheResponseIO::AppCacheResponseIO(
    int64 response_id, disk_cache::Backend* disk_cache)
    : response_id_(response_id), disk_cache_(disk_cache),
      entry_(NULL), user_callback_(NULL),
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

void AppCacheResponseReader::ReadInfo(HttpResponseInfoIOBuffer* info_buf,
                                      net::CompletionCallback* callback) {
  DCHECK(callback && !IsReadPending());
  DCHECK(info_buf && !info_buf->http_info.get());
  DCHECK(!buffer_.get() && !info_buffer_.get());

  user_callback_ = callback;  // cleared on completion

  if (!OpenEntryIfNeeded()) {
    ScheduleIOCompletionCallback(net::ERR_CACHE_MISS);
    return;
  }

  int size = entry_->GetDataSize(kResponseInfoIndex);
  if (size <= 0) {
    ScheduleIOCompletionCallback(net::ERR_CACHE_MISS);
    return;
  }

  info_buffer_ = info_buf;
  buffer_ = new net::IOBuffer(size);
  ReadRaw(kResponseInfoIndex, 0, buffer_.get(), size);
}

void AppCacheResponseReader::ReadData(net::IOBuffer* buf, int buf_len,
                                      net::CompletionCallback* callback) {
  DCHECK(callback && !IsReadPending());
  DCHECK(buf && (buf_len >= 0));
  DCHECK(!buffer_.get() && !info_buffer_.get());

  user_callback_ = callback;  // cleared on completion

  if (!OpenEntryIfNeeded()) {
    ScheduleIOCompletionCallback(net::ERR_CACHE_MISS);
    return;
  }

  buffer_ = buf;
  if (read_position_ + buf_len > range_length_) {
    // TODO(michaeln): What about integer overflows?
    DCHECK(range_length_ >= read_position_);
    buf_len = range_length_ - read_position_;
  }
  ReadRaw(kResponseContentIndex, range_offset_ + read_position_,
          buf, buf_len);
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
    } else {
      read_position_ += result;
    }
  }
  InvokeUserCompletionCallback(result);
}

bool AppCacheResponseReader::OpenEntryIfNeeded() {
  if (!entry_ && disk_cache_)
    disk_cache_->OpenEntry(response_key(response_id_), &entry_);
  return entry_ ? true : false;
}


// AppCacheResponseWriter ----------------------------------------------

void AppCacheResponseWriter::WriteInfo(HttpResponseInfoIOBuffer* info_buf,
                                       net::CompletionCallback* callback) {
  DCHECK(callback && !IsWritePending());
  DCHECK(info_buf && info_buf->http_info.get());
  DCHECK(!buffer_.get() && !info_buffer_.get());

  user_callback_ = callback;  // cleared on completion

  if (!CreateEntryIfNeeded()) {
    ScheduleIOCompletionCallback(net::ERR_FAILED);
    return;
  }

  const bool kSkipTransientHeaders = true;
  const bool kTruncated = false;
  Pickle* pickle = new Pickle;
  info_buf->http_info->Persist(pickle, kSkipTransientHeaders, kTruncated);
  info_buffer_ = info_buf;
  write_amount_ = static_cast<int>(pickle->size());
  buffer_ = new WrappedPickleIOBuffer(pickle);  // takes ownership of pickle
  WriteRaw(kResponseInfoIndex, 0, buffer_, write_amount_);
}

void AppCacheResponseWriter::WriteData(net::IOBuffer* buf, int buf_len,
                                       net::CompletionCallback* callback) {
  DCHECK(callback && !IsWritePending());
  DCHECK(buf && (buf_len >= 0));
  DCHECK(!buffer_.get() && !info_buffer_.get());

  user_callback_ = callback;  // cleared on completion

  if (!CreateEntryIfNeeded()) {
    ScheduleIOCompletionCallback(net::ERR_FAILED);
    return;
  }

  buffer_ = buf;
  write_amount_ = buf_len;
  WriteRaw(kResponseContentIndex, write_position_, buf, buf_len);
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

bool AppCacheResponseWriter::CreateEntryIfNeeded() {
  if (entry_)
    return true;
  if (!disk_cache_)
    return false;
  std::string key(response_key(response_id_));
  if (!disk_cache_->CreateEntry(key, &entry_)) {
    // We may try to overrite existing entries.
    DCHECK(!entry_);
    disk_cache_->DoomEntry(key);
    disk_cache_->CreateEntry(key, &entry_);
  }
  return entry_ ? true : false;
}

}  // namespace appcache

