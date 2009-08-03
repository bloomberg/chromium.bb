/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

//
// GzCompressor compresses a byte stream using gzip compression
// calling the client's ProcessBytes() method with the compressed stream
//

#include "import/cross/gz_compressor.h"

#include <assert.h>
#include "import/cross/memory_buffer.h"
#include "import/cross/memory_stream.h"
#include "zutil.h"

const size_t kChunkSize = 16384;

namespace o3d {

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
GzCompressor::GzCompressor(StreamProcessor *callback_client)
    : stream_is_closed_(false),
      callback_client_(callback_client) {
  strm_.zalloc = Z_NULL;
  strm_.zfree = Z_NULL;
  strm_.opaque = Z_NULL;

  // Store this, so we can later check if it's OK to start processing
  int result = deflateInit2(
      &strm_,
      Z_DEFAULT_COMPRESSION,
      Z_DEFLATED,
      MAX_WBITS + 16,  // 16 means write out gzip header/trailer
      DEF_MEM_LEVEL,
      Z_DEFAULT_STRATEGY);

  initialized_ = result == Z_OK;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void GzCompressor::Close(bool success) {
  if (!stream_is_closed_) {
    // Flush the compression stream
    MemoryReadStream stream(NULL, 0);
    CompressBytes(&stream, 0, true);

    // Deallocate resources
    deflateEnd(&strm_);
    stream_is_closed_ = true;
  }

  callback_client_->Close(success);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
GzCompressor::~GzCompressor() {
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
StreamProcessor::Status GzCompressor::ProcessBytes(MemoryReadStream *stream,
                                                   size_t bytes_to_process) {
  // Basic sanity check
  if (stream->GetDirectMemoryPointer() == NULL || bytes_to_process == 0) {
    return FAILURE;
  }

  return CompressBytes(stream, bytes_to_process, false);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
StreamProcessor::Status GzCompressor::CompressBytes(MemoryReadStream *stream,
                                                    size_t bytes_to_process,
                                                    bool flush) {
  // Don't even bother trying if we didn't get initialized properly
  if (!initialized_) return FAILURE;

  uint8 out[kChunkSize];

  // Don't try to read more than our stream has
  size_t remaining = stream->GetRemainingByteCount();
  if (bytes_to_process > remaining) {
    return FAILURE;
  }

  // Use direct memory access on the MemoryStream object
  const uint8 *input_data = stream->GetDirectMemoryPointer();
  stream->Skip(bytes_to_process);

  // Fill out the zlib z_stream struct
  strm_.avail_in = bytes_to_process;
  strm_.next_in = const_cast<uint8*>(input_data);

  // We need to flush the stream when we reach the end
  int flush_code = flush ? Z_FINISH : Z_NO_FLUSH;

  // Run deflate() on input until output buffer not full
  int result;
  do {
    strm_.avail_out = kChunkSize;
    strm_.next_out = out;

    result = deflate(&strm_, flush_code);
    if (result == Z_STREAM_ERROR)
      return FAILURE;

    size_t have = kChunkSize - strm_.avail_out;

    // Callback with the compressed byte stream
    MemoryReadStream decompressed_stream(out, have);
    if (have > 0 && callback_client_) {
      if (callback_client_->ProcessBytes(&decompressed_stream,
                                         have) == FAILURE) {
        return FAILURE;
      }
    }
  } while (strm_.avail_out == 0);

  return result == Z_OK ? IN_PROGRESS : FAILURE;
}

}  // namespace o3d
