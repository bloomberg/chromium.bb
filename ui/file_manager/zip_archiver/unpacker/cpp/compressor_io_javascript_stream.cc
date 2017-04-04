// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "compressor_io_javascript_stream.h"

#include <limits>
#include <thread>

#include "archive.h"
#include "ppapi/cpp/logging.h"

CompressorIOJavaScriptStream::CompressorIOJavaScriptStream(
    JavaScriptCompressorRequestorInterface* requestor)
    : requestor_(requestor) {
  pthread_mutex_init(&shared_state_lock_, NULL);
  pthread_cond_init(&available_data_cond_, NULL);
  pthread_cond_init(&data_written_cond_, NULL);

  pthread_mutex_lock(&shared_state_lock_);
  available_data_ = false;
  pthread_mutex_unlock(&shared_state_lock_);
}

CompressorIOJavaScriptStream::~CompressorIOJavaScriptStream() {
  pthread_cond_destroy(&data_written_cond_);
  pthread_cond_destroy(&available_data_cond_);
  pthread_mutex_destroy(&shared_state_lock_);
};

int64_t CompressorIOJavaScriptStream::Write(int64_t byte_to_write,
    const pp::VarArrayBuffer& buffer) {
  pthread_mutex_lock(&shared_state_lock_);
  requestor_->WriteChunkRequest(byte_to_write, buffer);

  pthread_cond_wait(&data_written_cond_, &shared_state_lock_);

  int64_t written_bytes = written_bytes_;
  pthread_mutex_unlock(&shared_state_lock_);

  return written_bytes;
}

void CompressorIOJavaScriptStream::WriteChunkDone(int64_t written_bytes) {
  pthread_mutex_lock(&shared_state_lock_);
  written_bytes_ = written_bytes;
  pthread_cond_signal(&data_written_cond_);
  pthread_mutex_unlock(&shared_state_lock_);
}

int64_t CompressorIOJavaScriptStream::Read(int64_t bytes_to_read,
                                           char* destination_buffer) {
  pthread_mutex_lock(&shared_state_lock_);

  destination_buffer_ = destination_buffer;
  requestor_->ReadFileChunkRequest(bytes_to_read);

  while (!available_data_) {
    pthread_cond_wait(&available_data_cond_, &shared_state_lock_);
  }

  int64_t read_bytes = read_bytes_;
  available_data_ = false;
  pthread_mutex_unlock(&shared_state_lock_);
  return read_bytes;
}

void CompressorIOJavaScriptStream::ReadFileChunkDone(int64_t read_bytes,
      pp::VarArrayBuffer* array_buffer) {
  pthread_mutex_lock(&shared_state_lock_);

  // JavaScript sets a negative value in read_bytes if an error occurred while
  // reading a chunk.
  if (read_bytes >= 0) {
    char* array_buffer_data = static_cast<char*>(array_buffer->Map());
    memcpy(destination_buffer_, array_buffer_data, read_bytes);
    array_buffer->Unmap();
  }

  read_bytes_ = read_bytes;
  available_data_ = true;
  pthread_cond_signal(&available_data_cond_);
  pthread_mutex_unlock(&shared_state_lock_);
}
