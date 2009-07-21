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


#include "import/cross/memory_buffer.h"
#include "import/cross/tar_generator.h"

using std::string;

#if defined(OS_WIN)
  #define snprintf _snprintf
#endif

namespace o3d {

const int kMaxFilenameSizeOldFormat = 100;

const int kFileNameOffset         = 0;
const int kFileModeOffset         = 100;
const int kUserIDOffset           = 108;
const int kGroupIDOffset          = 116;
const int kFileSizeOffset         = 124;
const int kModifyTimeOffset       = 136;
const int kHeaderCheckSumOffset   = 148;
const int kLinkFlagOffset         = 156;
const int kMagicOffset            = 257;
const int kUserNameOffset         = 265;
const int kGroupNameOffset        = 297;
// This is the name GNU Tar puts in a header block if the block is really
// a long filename block.
static const char* kLongLink = "././@LongLink";

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool TarGenerator::AddFile(const String &file_name, size_t file_size) {
  if (!AddDirectoryEntryIfNeeded(file_name)) {
    return false;
  }
  return AddEntry(file_name, file_size, false);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool TarGenerator::AddDirectory(const String &file_name) {
  return AddEntry(file_name, 0, true);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// We keep a map so we add a particular directory entry only once
bool TarGenerator::AddDirectoryEntryIfNeeded(const String &file_name) {
  string::size_type index = file_name.find_last_of('/');

  if (index != string::npos) {
    String dir_name = file_name.substr(0, index + 1);  // keep the '/' at end
    if (!directory_map_[dir_name]) {
      directory_map_[dir_name] = true;
      return AddDirectory(dir_name);
    }
  }
  return true;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


void TarGenerator::WriteHeader(const String& file_name,
                               size_t file_size,
                               char type,
                               int mode,
                               int user_id,
                               int group_id,
                               int mod_time) {
  // first write out last data block from last file (if any)
  FlushDataBuffer(true);

  // next fill out a tar header
  MemoryBuffer<uint8> header(TAR_HEADER_SIZE);  // initializes header to zeroes

  // leave room for NULL-termination
  uint8 *h = header;
  char *p = reinterpret_cast<char*>(h);

  // File name
  strncpy(p, file_name.c_str(), kMaxFilenameSizeOldFormat - 1);

  // File mode
  ::snprintf(p + kFileModeOffset, 8, "%07o", mode);

  // UserID
  ::snprintf(p + kUserIDOffset, 8, "%07o", user_id);

  // GroupID
  ::snprintf(p + kGroupIDOffset, 8, "%07o", group_id);

  // File size
  ::snprintf(p + kFileSizeOffset, 12, "%011o", file_size);

  // Modification time
  // TODO: write the correct current time here...
  ::snprintf(p + kModifyTimeOffset, 12, "%07o", mod_time);

  // Initialize Header checksum so check sum can be computed
  // by ComputeCheckSum() which will fill in the value here
  ::memset(p + kHeaderCheckSumOffset, 32, 8);

  // We only support ordinary files,directories and long filename blogs, which
  // is fine for our use case
  p[kLinkFlagOffset] = type;

  // Magic offset
  ::snprintf(p + kMagicOffset, 8, "ustar  ");

  // User name
  ::snprintf(p + kUserNameOffset, 32, "guest");

  // Group name
  ::snprintf(p + kGroupNameOffset, 32, "staff");

  // This has to be done at the end
  ComputeCheckSum(header);

  if (callback_client_) {
    MemoryReadStream stream(header, TAR_HEADER_SIZE);
    callback_client_->ProcessBytes(&stream, TAR_HEADER_SIZE);
  }
}


bool TarGenerator::AddEntry(const String &file_name,
                            size_t file_size,
                            bool is_directory) {
  // If filename is longer 99 chars, use the GNU format to write out a longer
  // filename.
  if (file_name.size() >= kMaxFilenameSizeOldFormat) {
    WriteHeader(kLongLink,
                file_name.size(),
                'L',
                0,
                0,
                0,
                0);

    MemoryReadStream stream(
        reinterpret_cast<const uint8*>(file_name.c_str()), file_name.size());
    AddFileBytes(&stream, file_name.size());
  }

  WriteHeader(file_name,
              file_size,
              is_directory ? '5' : '0',
              is_directory ? 0755 : 0644,
              0765,
              0204,
              011131753141);

  return true;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void TarGenerator::ComputeCheckSum(uint8 *header) {
  unsigned int checksum = 0;
  for (int i = 0; i < TAR_HEADER_SIZE; ++i) {
    checksum += header[i];
  }
  snprintf(reinterpret_cast<char*>(header + kHeaderCheckSumOffset),
           8, "%06o\0\0", checksum);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
int TarGenerator::AddFileBytes(MemoryReadStream *stream, size_t n) {
  if (callback_client_) {
    FlushDataBuffer(false);  // flush any old data sitting around

    // we'll directly write as much of the data as we can, writing full blocks
    int nblocks = n / TAR_BLOCK_SIZE;
    size_t direct_bytes_to_write = nblocks * TAR_BLOCK_SIZE;
    if (direct_bytes_to_write > 0) {
      callback_client_->ProcessBytes(stream, direct_bytes_to_write);
    }

    // now, buffer the remainder (less than TAR_BLOCK_SIZE)
    size_t remainder = n - direct_bytes_to_write;

    const uint8 *p = stream->GetDirectMemoryPointer();
    data_buffer_stream_.Write(p, remainder);
    stream->Skip(remainder);
  }

  return 0;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void TarGenerator::FlushDataBuffer(bool flush_padding_zeroes) {
  size_t buffered_bytes = data_buffer_stream_.GetStreamPosition();
  if (buffered_bytes > 0 && callback_client_) {
    // write out the complete data block (which may be zero padded at the end)
    size_t bytes_to_flush =
        flush_padding_zeroes ? TAR_BLOCK_SIZE : buffered_bytes;

    MemoryReadStream stream(data_block_buffer_, bytes_to_flush);
    callback_client_->ProcessBytes(&stream, bytes_to_flush);

    // get ready to start buffering next data block
    data_block_buffer_.Clear();
    data_buffer_stream_.Seek(0);
  }
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void TarGenerator::Finalize() {
  FlushDataBuffer(true);
}

}  // namespace o3d
