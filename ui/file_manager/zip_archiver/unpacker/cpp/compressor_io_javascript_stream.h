// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPRESSOR_IO_JAVSCRIPT_STREAM_H_
#define COMPRESSOR_IO_JAVSCRIPT_STREAM_H_

#include <pthread.h>
#include <string>

#include "archive.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/var_array_buffer.h"
#include "ppapi/utility/completion_callback_factory.h"
#include "ppapi/utility/threading/lock.h"
#include "ppapi/utility/threading/simple_thread.h"

#include "compressor_stream.h"
#include "javascript_compressor_requestor_interface.h"

class CompressorIOJavaScriptStream : public CompressorStream {
 public:
  CompressorIOJavaScriptStream(
      JavaScriptCompressorRequestorInterface* requestor);

  virtual ~CompressorIOJavaScriptStream();

  virtual int64_t Write(int64_t bytes_to_read,
                        const pp::VarArrayBuffer& buffer);

  virtual void WriteChunkDone(int64_t write_bytes);

  virtual int64_t Read(int64_t bytes_to_read, char* destination_buffer);

  virtual void ReadFileChunkDone(int64_t read_bytes,
                                 pp::VarArrayBuffer* buffer);

private:
  // A requestor that makes calls to JavaScript to read and write chunks.
  JavaScriptCompressorRequestorInterface* requestor_;

  pthread_mutex_t shared_state_lock_;
  pthread_cond_t available_data_cond_;
  pthread_cond_t data_written_cond_;

  // The bytelength of the data written onto the archive for the last write
  // chunk request. If this value is negative, some error occurred when writing
  // a chunk in JavaScript.
  int64_t written_bytes_;

  // The bytelength of the data read from the entry for the last read file chunk
  // request. If this value is negative, some error occurred when reading a
  // chunk in JavaScript.
  int64_t read_bytes_;

  // True if destination_buffer_ is available.
  bool available_data_;

  // Stores the data read from JavaScript.
  char* destination_buffer_;
};

#endif  // COMPRESSOR_IO_JAVSCRIPT_STREAM_H_
