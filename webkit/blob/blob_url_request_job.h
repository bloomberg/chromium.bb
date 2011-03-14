// Copyright (c) 20010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BLOB_BLOB_URL_REQUEST_JOB_H_
#define WEBKIT_BLOB_BLOB_URL_REQUEST_JOB_H_

#include "base/platform_file.h"
#include "base/ref_counted.h"
#include "base/scoped_callback_factory.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "net/base/completion_callback.h"
#include "net/http/http_byte_range.h"
#include "net/url_request/url_request_job.h"
#include "webkit/blob/blob_data.h"

namespace base {
class MessageLoopProxy;
struct PlatformFileInfo;
}

namespace net {
class FileStream;
}

namespace webkit_blob {

// A request job that handles reading blob URLs.
class BlobURLRequestJob : public net::URLRequestJob {
 public:
  BlobURLRequestJob(net::URLRequest* request,
                    BlobData* blob_data,
                    base::MessageLoopProxy* resolving_message_loop_proxy);
  virtual ~BlobURLRequestJob();

  // net::URLRequestJob methods.
  virtual void Start();
  virtual void Kill();
  virtual bool ReadRawData(net::IOBuffer* buf, int buf_size, int* bytes_read);
  virtual bool GetMimeType(std::string* mime_type) const;
  virtual void GetResponseInfo(net::HttpResponseInfo* info);
  virtual int GetResponseCode() const;
  virtual void SetExtraRequestHeaders(const net::HttpRequestHeaders& headers);

 private:
  void CloseStream();
  void ResolveFile(const FilePath& file_path);
  void CountSize();
  void Seek(int64 offset);
  void AdvanceItem();
  void AdvanceBytesRead(int result);
  int ComputeBytesToRead() const;
  bool ReadLoop(int* bytes_read);
  bool ReadItem();
  bool ReadBytes(const BlobData::Item& item);
  bool DispatchReadFile(const BlobData::Item& item);
  bool ReadFile(const BlobData::Item& item);
  void HeadersCompleted(int status_code, const std::string& status_txt);
  int ReadCompleted();
  void NotifySuccess();
  void NotifyFailure(int);

  void DidStart();
  void DidResolve(base::PlatformFileError rv,
                  const base::PlatformFileInfo& file_info);
  void DidOpen(base::PlatformFileError rv,
               base::PassPlatformFile file,
               bool created);
  void DidRead(int result);

  base::ScopedCallbackFactory<BlobURLRequestJob> callback_factory_;
  scoped_refptr<BlobData> blob_data_;
  scoped_refptr<base::MessageLoopProxy> file_thread_proxy_;
  net::CompletionCallbackImpl<BlobURLRequestJob> io_callback_;
  std::vector<int64> item_length_list_;
  scoped_ptr<net::FileStream> stream_;
  size_t item_index_;
  int64 total_size_;
  int64 current_item_offset_;
  int64 remaining_bytes_;
  scoped_refptr<net::IOBuffer> read_buf_;
  int read_buf_offset_;
  int read_buf_size_;
  int read_buf_remaining_bytes_;
  int bytes_to_read_;
  bool error_;
  bool headers_set_;
  bool byte_range_set_;
  net::HttpByteRange byte_range_;
  scoped_ptr<net::HttpResponseInfo> response_info_;
  ScopedRunnableMethodFactory<BlobURLRequestJob> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlobURLRequestJob);
};

}  // namespace webkit_blob

#endif  // WEBKIT_BLOB_BLOB_URL_REQUEST_JOB_H_
