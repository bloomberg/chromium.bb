// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BLOB_BLOB_URL_REQUEST_JOB_H_
#define WEBKIT_BLOB_BLOB_URL_REQUEST_JOB_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "net/http/http_byte_range.h"
#include "net/url_request/url_request_job.h"
#include "webkit/blob/blob_data.h"
#include "webkit/blob/blob_export.h"

namespace base {
class MessageLoopProxy;
struct PlatformFileInfo;
}

namespace net {
class DrainableIOBuffer;
class FileStream;
class IOBuffer;
}

namespace webkit_blob {

// A request job that handles reading blob URLs.
class BLOB_EXPORT BlobURLRequestJob : public net::URLRequestJob {
 public:
  BlobURLRequestJob(net::URLRequest* request,
                    BlobData* blob_data,
                    base::MessageLoopProxy* resolving_message_loop_proxy);
  virtual ~BlobURLRequestJob();

  // net::URLRequestJob methods.
  virtual void Start() OVERRIDE;
  virtual void Kill() OVERRIDE;
  virtual bool ReadRawData(net::IOBuffer* buf,
                           int buf_size,
                           int* bytes_read) OVERRIDE;
  virtual bool GetMimeType(std::string* mime_type) const OVERRIDE;
  virtual void GetResponseInfo(net::HttpResponseInfo* info) OVERRIDE;
  virtual int GetResponseCode() const OVERRIDE;
  virtual void SetExtraRequestHeaders(
      const net::HttpRequestHeaders& headers) OVERRIDE;

 private:
  // For preparing for read: get the size, apply the range and perform seek.
  void DidStart();
  void CountSize();
  void GetFileItemInfo(const FilePath& file_path);
  void DidGetFileItemInfo(base::PlatformFileError rv,
                          const base::PlatformFileInfo& file_info);
  void Seek(int64 offset);

  // For reading the blob.
  bool ReadLoop(int* bytes_read);
  bool ReadItem();
  void AdvanceItem();
  void AdvanceBytesRead(int result);
  bool ReadBytesItem(const BlobData::Item& item);
  bool ReadFileItem(const BlobData::Item& item);

  void DidOpenFile(base::PlatformFileError rv,
                   base::PassPlatformFile file,
                   bool created);
  bool ReadFileStream();
  void DidReadFileStream(int result);
  void CloseFileStream();

  int ComputeBytesToRead() const;
  int BytesReadCompleted();

  void NotifySuccess();
  void NotifyFailure(int);
  void HeadersCompleted(int status_code, const std::string& status_txt);

  base::WeakPtrFactory<BlobURLRequestJob> weak_factory_;
  scoped_refptr<BlobData> blob_data_;
  scoped_refptr<base::MessageLoopProxy> file_thread_proxy_;
  std::vector<int64> item_length_list_;
  scoped_ptr<net::FileStream> stream_;
  size_t item_index_;
  int64 total_size_;
  int64 current_item_offset_;
  int64 remaining_bytes_;
  scoped_refptr<net::DrainableIOBuffer> read_buf_;
  int bytes_to_read_;
  bool error_;
  bool headers_set_;
  bool byte_range_set_;
  net::HttpByteRange byte_range_;
  scoped_ptr<net::HttpResponseInfo> response_info_;

  DISALLOW_COPY_AND_ASSIGN(BlobURLRequestJob);
};

}  // namespace webkit_blob

#endif  // WEBKIT_BLOB_BLOB_URL_REQUEST_JOB_H_
