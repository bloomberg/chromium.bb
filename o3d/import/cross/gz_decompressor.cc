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
// GzDecompressor decompresses a gzip compressed byte stream
// calling the client's ProcessBytes() method with the uncompressed stream
//

#include "import/cross/gz_decompressor.h"

#include <assert.h>
#include "import/cross/memory_stream.h"

const size_t kChunkSize = 16384;

namespace o3d {

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
GzDecompressor::GzDecompressor(StreamProcessor *callback_client)
    : callback_client_(callback_client) {
  // Initialize inflate state
  strm_.zalloc = Z_NULL;
  strm_.zfree = Z_NULL;
  strm_.opaque = Z_NULL;
  strm_.avail_in = 0;
  strm_.next_in = Z_NULL;

  // Store this, so we can later check if it's OK to start processing
  initialized_ = inflateInit2(&strm_, 31) == Z_OK;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
GzDecompressor::~GzDecompressor() {
  inflateEnd(&strm_);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
StreamProcessor::Status GzDecompressor::ProcessBytes(MemoryReadStream *stream,
                                                     size_t bytes_to_process) {
  // Don't even bother trying if we didn't get initialized properly
  if (!initialized_) return FAILURE;

  uint8 out[kChunkSize];
  int result;
  size_t have;

  // Don't try to read more than our stream has
  size_t remaining = stream->GetRemainingByteCount();
  if (bytes_to_process > remaining) {
    return FAILURE;
  }

  // Use direct memory access on the MemoryStream object
  strm_.avail_in = bytes_to_process;
  strm_.next_in = const_cast<uint8*>(stream->GetDirectMemoryPointer());
  stream->Skip(bytes_to_process);

  // Run inflate() on input until output buffer not full
  do {
    strm_.avail_out = kChunkSize;
    strm_.next_out = out;

    result = inflate(&strm_, Z_NO_FLUSH);

    // error check here - return error codes if necessary
    assert(result != Z_STREAM_ERROR);  /* state not clobbered */
    switch (result) {
      case Z_NEED_DICT:
      case Z_DATA_ERROR:
      case Z_MEM_ERROR:
        return FAILURE;
    }

    have = kChunkSize - strm_.avail_out;

    // Callback with the decompressed byte stream
    MemoryReadStream decompressed_stream(out, have);
    if (callback_client_) {
      if (callback_client_->ProcessBytes(&decompressed_stream,
                                         have) == FAILURE) {
        return FAILURE;
      }
    }
  } while (strm_.avail_out == 0);

  switch (result) {
    case Z_OK:
    case Z_BUF_ERROR: // Zlib docs say this is expected.
      return IN_PROGRESS;
    case Z_STREAM_END:
      return SUCCESS;
    default:
      return FAILURE;
  }
}

void GzDecompressor::Close(bool success) {
  callback_client_->Close(success);
}

}  // namespace o3d
