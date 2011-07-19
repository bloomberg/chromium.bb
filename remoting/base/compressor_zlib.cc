// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/compressor_zlib.h"

#if defined(USE_SYSTEM_ZLIB)
#include <zlib.h>
#else
#include "third_party/zlib/zlib.h"
#endif
#include "base/logging.h"

namespace remoting {

CompressorZlib::CompressorZlib() {
  Reset();
}

CompressorZlib::~CompressorZlib() {
  deflateEnd(stream_.get());
}

void CompressorZlib::Reset() {
  if (stream_.get())
    deflateEnd(stream_.get());

  stream_.reset(new z_stream());

  stream_->next_in = Z_NULL;
  stream_->zalloc = Z_NULL;
  stream_->zfree = Z_NULL;
  stream_->opaque = Z_NULL;

  deflateInit(stream_.get(), Z_BEST_SPEED);
}

bool CompressorZlib::Process(const uint8* input_data, int input_size,
                             uint8* output_data, int output_size,
                             CompressorFlush flush, int* consumed,
                             int* written) {
  DCHECK_GT(output_size, 0);

  // Setup I/O parameters.
  stream_->avail_in = input_size;
  stream_->next_in = (Bytef*)input_data;
  stream_->avail_out = output_size;
  stream_->next_out = (Bytef*)output_data;

  int z_flush = 0;
  if (flush == CompressorSyncFlush) {
    z_flush = Z_SYNC_FLUSH;
  } else if (flush == CompressorFinish) {
    z_flush = Z_FINISH;
  } else if (flush == CompressorNoFlush) {
    z_flush = Z_NO_FLUSH;
  } else {
    NOTREACHED() << "Unsupported flush mode";
  }

  int ret = deflate(stream_.get(), z_flush);
  if (ret == Z_STREAM_ERROR) {
    NOTREACHED() << "zlib compression failed";
  }

  *consumed = input_size - stream_->avail_in;
  *written = output_size - stream_->avail_out;

  // If |ret| equals Z_STREAM_END we have reached the end of stream.
  // If |ret| equals Z_BUF_ERROR we have the end of the synchronication point.
  // For these two cases we need to stop compressing.
  if (ret == Z_OK) {
    return true;
  } else if (ret == Z_STREAM_END) {
    return false;
  } else if (ret == Z_BUF_ERROR) {
    return stream_->avail_out == 0;
  } else {
    NOTREACHED() << "Unexpected zlib error: " << ret;
    return false;
  }
}

}  // namespace remoting
