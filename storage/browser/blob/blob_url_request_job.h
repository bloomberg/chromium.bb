// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_URL_REQUEST_JOB_H_
#define STORAGE_BROWSER_BLOB_BLOB_URL_REQUEST_JOB_H_

#include <map>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_job.h"
#include "storage/browser/blob/blob_data_snapshot.h"
#include "storage/browser/storage_browser_export.h"

namespace base {
class MessageLoopProxy;
}

namespace storage {
class FileSystemContext;
}

namespace net {
class DrainableIOBuffer;
class IOBuffer;
}

namespace storage {

class FileStreamReader;

// A request job that handles reading blob URLs.
class STORAGE_EXPORT BlobURLRequestJob
    : public net::URLRequestJob {
 public:
  BlobURLRequestJob(net::URLRequest* request,
                    net::NetworkDelegate* network_delegate,
                    scoped_ptr<BlobDataSnapshot> blob_data,
                    storage::FileSystemContext* file_system_context,
                    base::MessageLoopProxy* resolving_message_loop_proxy);

  // net::URLRequestJob methods.
  void Start() override;
  void Kill() override;
  bool ReadRawData(net::IOBuffer* buf, int buf_size, int* bytes_read) override;
  bool GetMimeType(std::string* mime_type) const override;
  void GetResponseInfo(net::HttpResponseInfo* info) override;
  int GetResponseCode() const override;
  void SetExtraRequestHeaders(const net::HttpRequestHeaders& headers) override;

 protected:
  ~BlobURLRequestJob() override;

 private:
  typedef std::map<size_t, FileStreamReader*> IndexToReaderMap;

  // For preparing for read: get the size, apply the range and perform seek.
  void DidStart();
  bool AddItemLength(size_t index, int64 item_length);
  void CountSize();
  void DidCountSize(int error);
  void DidGetFileItemLength(size_t index, int64 result);
  void Seek(int64 offset);

  // For reading the blob.
  bool ReadLoop(int* bytes_read);
  bool ReadItem();
  void AdvanceItem();
  void AdvanceBytesRead(int result);
  bool ReadBytesItem(const BlobDataItem& item, int bytes_to_read);
  bool ReadFileItem(FileStreamReader* reader, int bytes_to_read);

  void DidReadFile(int chunk_number, int result);
  void DeleteCurrentFileReader();

  int ComputeBytesToRead() const;
  int BytesReadCompleted();

  // These methods convert the result of blob data reading into response headers
  // and pass it to URLRequestJob's NotifyDone() or NotifyHeadersComplete().
  void NotifySuccess();
  void NotifyFailure(int);
  void HeadersCompleted(net::HttpStatusCode status_code);

  // Returns a FileStreamReader for a blob item at |index|.
  // If the item at |index| is not of file this returns NULL.
  FileStreamReader* GetFileStreamReader(size_t index);

  // Creates a FileStreamReader for the item at |index| with additional_offset.
  void CreateFileStreamReader(size_t index, int64 additional_offset);

  scoped_ptr<BlobDataSnapshot> blob_data_;

  // Variables for controlling read from |blob_data_|.
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  scoped_refptr<base::MessageLoopProxy> file_thread_proxy_;
  std::vector<int64> item_length_list_;
  int64 total_size_;
  int64 remaining_bytes_;
  int pending_get_file_info_count_;
  IndexToReaderMap index_to_reader_;
  size_t current_item_index_;
  int64 current_item_offset_;

  // Holds the buffer for read data with the IOBuffer interface.
  scoped_refptr<net::DrainableIOBuffer> read_buf_;

  // Is set when NotifyFailure() is called and reset when DidStart is called.
  bool error_;

  bool byte_range_set_;
  net::HttpByteRange byte_range_;

  // Used to create unique id's for tracing.
  int current_file_chunk_number_;

  scoped_ptr<net::HttpResponseInfo> response_info_;

  base::WeakPtrFactory<BlobURLRequestJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlobURLRequestJob);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_BLOB_BLOB_URL_REQUEST_JOB_H_
