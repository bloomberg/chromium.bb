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


#include "import/cross/targz_processor.h"
#include "tests/common/win/testing_common.h"
#include "tests/common/cross/test_utils.h"

namespace o3d {

class TarProcessorTest : public testing::Test {
};

// We verify that the tar file contains exactly these filenames
static const char *kFilename1 = "test/file1";
static const char *kFilename2 =
    "test/file1ThisIsAFilenameLongerThen100Chars"
    "ThisIsAFilenameLongerThen100Chars"
    "ThisIsAFilenameLongerThen100CharsThisIsAFilenameLongerThen100Chars";
static const char *kFilename3 = "test/file2";
static const char *kFilename4 = "test/file3";

// With each file having these exact contents

// we should receive these (and exactly these bytes in this order)
static const char *kConcatenatedContents =
  "the cat in the hat\n"       // file 1 contents.
  "this file has a long name"  // file 2 contents.
  "abracadabra\n"              // file 3 contents.
  "I think therefore I am\n"   // file 4 contents.
  "";                          // end

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class TarTestClient : public ArchiveCallbackClient {
 public:
  explicit TarTestClient() : file_count_(0), index_(0) {}
  // ArchiveCallbackClient methods
  virtual void ReceiveFileHeader(const ArchiveFileInfo &file_info);
  virtual bool ReceiveFileData(MemoryReadStream *stream, size_t nbytes);

  virtual void Close(bool success) {
    closed_ = true;
    success_ = success;
  }

  int GetFileCount() const { return file_count_; }
  size_t GetNumTotalBytesReceived() const { return index_; }

  bool closed() const {
    return closed_;
  }

  bool success() const {
    return success_;
  }

 private:
  int file_count_;
  int index_;
  bool closed_;
  bool success_;
};

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void TarTestClient::ReceiveFileHeader(const ArchiveFileInfo &file_info) {
  // We get called one time for each file in the archive

  // Check that the filenames match our expectation
  switch (file_count_) {
    case 0:
      EXPECT_TRUE(!strcmp(kFilename1, file_info.GetFileName().c_str()));
      break;
    case 1:
      EXPECT_TRUE(!strcmp(kFilename2, file_info.GetFileName().c_str()));
      break;
    case 2:
      EXPECT_TRUE(!strcmp(kFilename3, file_info.GetFileName().c_str()));
      break;
    case 3:
      EXPECT_TRUE(!strcmp(kFilename4, file_info.GetFileName().c_str()));
      break;
  }

  file_count_++;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool TarTestClient::ReceiveFileData(MemoryReadStream *stream, size_t nbytes) {
  const char *p = reinterpret_cast<const char*>(
      stream->GetDirectMemoryPointer());

  // Note: ReceiveFileData() may be called multiple times for each file, until
  // the complete file contents have been given...

  // We're receiving three file, one after the other
  // The bytes we receive are the concatenated contents of the three files
  // (with calls to ReceiveFileHeader() separating each one)
  //
  EXPECT_TRUE(index_ + nbytes <= strlen(kConcatenatedContents));
  EXPECT_TRUE(!strncmp(kConcatenatedContents + index_, p, nbytes));

  index_ += nbytes;

  return true;
}

// Loads a tar file, runs it through the processor.
// In our callbacks, we verify that we receive three files with known contents
//
TEST_F(TarProcessorTest, LoadTarFile) {
  String filepath = *g_program_path + "/archive_files/test1.tar";

  // Read the test tar file into memory
  size_t file_size;
  uint8 *tar_data = test_utils::ReadFile(filepath, &file_size);
  ASSERT_TRUE(tar_data != NULL);

  // Gets header and file data callbacks
  TarTestClient callback_client;

  // The class we're testing...
  TarProcessor tar_processor(&callback_client);

  // Now that we've read the compressed file into memory, lets
  // feed it, a chunk at a time, into the tar_processor
  const int kChunkSize = 32;

  MemoryReadStream tar_stream(tar_data, file_size);
  int bytes_to_process = file_size;

  while (bytes_to_process > 0) {
    int bytes_this_time =
        bytes_to_process < kChunkSize ? bytes_to_process : kChunkSize;

    StreamProcessor::Status status = tar_processor.ProcessBytes(
        &tar_stream, bytes_this_time);
    EXPECT_TRUE(status != StreamProcessor::FAILURE);

    bytes_to_process -= bytes_this_time;
  }
  tar_processor.Close(true);

  EXPECT_TRUE(callback_client.closed());
  EXPECT_TRUE(callback_client.success());

  free(tar_data);
}

TEST_F(TarProcessorTest, PassesThroughFailure) {
  TarTestClient callback_client;
  TarProcessor tar_processor(&callback_client);
  tar_processor.Close(false);

  EXPECT_TRUE(callback_client.closed());
  EXPECT_FALSE(callback_client.success());
}

}  // namespace
