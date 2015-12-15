// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_UPLOAD_BLOB_ELEMENT_READER_H_
#define STORAGE_BROWSER_BLOB_UPLOAD_BLOB_ELEMENT_READER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/completion_callback.h"
#include "net/base/upload_element_reader.h"
#include "storage/browser/storage_browser_export.h"

namespace net {
class IOBuffer;
}

namespace storage {
class BlobDataHandle;
class BlobReader;

// This class is a wrapper around the BlobReader to make it conform
// to the net::UploadElementReader interface, and it also holds around the
// handle to the blob so it stays in memory while we read it.
class STORAGE_EXPORT UploadBlobElementReader
    : NON_EXPORTED_BASE(public net::UploadElementReader) {
 public:
  UploadBlobElementReader(scoped_ptr<BlobReader> reader,
                          scoped_ptr<BlobDataHandle> handle);
  ~UploadBlobElementReader() override;

  int Init(const net::CompletionCallback& callback) override;

  uint64_t GetContentLength() const override;

  uint64_t BytesRemaining() const override;

  bool IsInMemory() const override;

  int Read(net::IOBuffer* buf,
           int buf_length,
           const net::CompletionCallback& callback) override;

  const std::string& uuid() const;

 private:
  scoped_ptr<BlobReader> reader_;
  scoped_ptr<BlobDataHandle> handle_;

  DISALLOW_COPY_AND_ASSIGN(UploadBlobElementReader);
};

}  // namespace storage
#endif  // STORAGE_BROWSER_BLOB_UPLOAD_BLOB_ELEMENT_READER_H_
