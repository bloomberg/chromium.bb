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

#include <stdio.h>
#include <sys/stat.h>

#include "import/cross/archive_processor.h"

#include "base/logging.h"
#include "import/cross/memory_buffer.h"
#include "zlib.h"

const int kChunkSize = 16384;

namespace o3d {

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
StreamProcessor::Status ArchiveProcessor::ProcessEntireStream(
    MemoryReadStream *stream) {
  Status status;

  // decompress until deflate stream ends or error
  do {
    int remaining = stream->GetRemainingByteCount();

    int process_this_time = remaining < kChunkSize ? remaining : kChunkSize;
    status = ProcessBytes(stream, process_this_time);
  } while (status == IN_PROGRESS);

  return status;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
StreamProcessor::Status ArchiveProcessor::ProcessFile(const char *filename) {
  struct stat file_info;
  int result = stat(filename, &file_info);
  if (result != 0) return FAILURE;

  int file_length = file_info.st_size;
  if (file_length == 0) return FAILURE;

  MemoryBuffer<uint8> buffer;
  buffer.Allocate(file_length);
  uint8 *p = buffer;

  // Test by reading in a tar.gz file and sending through the
  // progressive streaming system
  FILE *fp = fopen(filename, "rb");
  if (!fp) return FAILURE;  // can't open file!
  fread(p, sizeof(uint8), file_length, fp);
  fclose(fp);

  MemoryReadStream stream(p, file_length);

  return ProcessEntireStream(&stream);
}

}  // namespace o3d
