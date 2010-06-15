// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/compression.h"

#include <assert.h>

namespace remoting {

static const int kZLIB_CHUNK = 256 * 1024;

ZCompressor::ZCompressor() {
  stream_.zalloc = Z_NULL;
  stream_.zfree = Z_NULL;
  stream_.opaque = Z_NULL;

  deflateInit(&stream_, Z_BEST_SPEED);
}

void ZCompressor::WriteInternal(char* buffer, int size, int flush) {
  stream_.avail_in = size;
  stream_.next_in = reinterpret_cast<Bytef*>(buffer);

  // Expand the internal buffer.
  while (true) {
    int last_size = buffer_.size();
    buffer_.resize(last_size + kZLIB_CHUNK);
    stream_.avail_out = kZLIB_CHUNK;
    stream_.next_out = reinterpret_cast<Bytef*>(&buffer_[last_size]);
    int ret = deflate(&stream_, flush);

    // Check ret before the assert so that we don't get an unused variable
    // warning in release builds.
    if (ret == Z_STREAM_ERROR) {
      assert(ret != Z_STREAM_ERROR);
    }

    // Shrink the size of the vector. It doesn't alter the capacity.
    int compressed = kZLIB_CHUNK - stream_.avail_out;
    buffer_.resize(last_size + compressed);
    if (!compressed)
      break;
  }
}

void ZCompressor::Write(char* buffer, int size) {
  WriteInternal(buffer, size, Z_NO_FLUSH);
}

void ZCompressor::Flush() {
  WriteInternal(NULL, 0, Z_FINISH);
  deflateEnd(&stream_);
}

int ZCompressor::GetCompressedSize() {
  return buffer_.size();
}

char* ZCompressor::GetCompressedData() {
  return &buffer_[0];
}

int ZCompressor::GetRawSize() {
  // I don't care about this.
  return 0;
}

ZDecompressor::ZDecompressor() {
  stream_.zalloc = Z_NULL;
  stream_.zfree = Z_NULL;
  stream_.opaque = Z_NULL;
  stream_.avail_in = 0;
  stream_.next_in = Z_NULL;

  inflateInit(&stream_);
}

void ZDecompressor::WriteInternal(char* buffer, int size, int flush) {
  stream_.avail_in = size;
  stream_.next_in = reinterpret_cast<Bytef*>(buffer);

  while (true) {
    int last_size = buffer_.size();
    buffer_.resize(last_size + kZLIB_CHUNK);
    stream_.avail_out = kZLIB_CHUNK;
    stream_.next_out = reinterpret_cast<Bytef*>(&buffer_[last_size]);
    int ret = inflate(&stream_, flush);

    // Check ret before the assert so that we don't get an unused variable
    // warning in release builds.
    if (ret == Z_STREAM_ERROR) {
      assert(ret != Z_STREAM_ERROR);
    }

    // Shrink the size of the vector. It doesn't alter the capacity.
    int decompressed = kZLIB_CHUNK - stream_.avail_out;
    buffer_.resize(last_size + decompressed);
    if (!decompressed)
      break;
  }
}

void ZDecompressor::Write(char* buffer, int size) {
  WriteInternal(buffer, size, Z_NO_FLUSH);
}

void ZDecompressor::Flush() {
  inflateEnd(&stream_);
}

char* ZDecompressor::GetRawData() {
  return &buffer_[0];
}

int ZDecompressor::GetRawSize() {
  return buffer_.size();
}

int ZDecompressor::GetCompressedSize() {
  // I don't care.
  return 0;
}

}  // namespace remoting
