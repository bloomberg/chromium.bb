// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_COMPRESSOR_ARCHIVE_MINIZIP_H_
#define CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_COMPRESSOR_ARCHIVE_MINIZIP_H_

#include <memory>
#include <string>

#include "chrome/browser/resources/chromeos/zip_archiver/cpp/compressor_archive.h"
#include "chrome/browser/resources/chromeos/zip_archiver/cpp/minizip_helpers.h"
#include "third_party/minizip/src/mz_strm.h"

class CompressorStream;

class CompressorArchiveMinizip : public CompressorArchive {
 public:
  explicit CompressorArchiveMinizip(CompressorStream* compressor_stream);

  ~CompressorArchiveMinizip() override;

  // Creates an archive object.
  bool CreateArchive() override;

  // Closes the archive.
  bool CloseArchive(bool has_error) override;

  // Cancels the compression process.
  void CancelArchive() override;

  // Adds an entry to the archive.
  bool AddToArchive(const std::string& filename,
                    int64_t file_size,
                    base::Time modification_time,
                    bool is_directory) override;

 private:
  static mz_stream_vtbl minizip_vtable;
  struct MinizipStream;

  // Stream functions used by minizip. In all cases, |compressor| points to
  // |this->stream_|.
  static int32_t MinizipWrite(void* stream, const void* buffer, int32_t length);
  static int64_t MinizipTell(void* stream);
  static int32_t MinizipSeek(void* stream, int64_t offset, int32_t origin);

  // Implementation of stream functions used by minizip.
  int32_t StreamWrite(const void* buffer, int32_t length);
  int64_t StreamTell();
  int32_t StreamSeek(int64_t offset, int32_t origin);

  // An instance that takes care of all IO operations.
  CompressorStream* compressor_stream_;

  // The minizip stream used to write the archive file.
  std::unique_ptr<MinizipStream> stream_;

  // The minizip correspondent archive object.
  ScopedMzZip zip_file_;

  // The buffer used to store the data read from JavaScript.
  std::unique_ptr<char[]> destination_buffer_;

  // The current offset of the zip archive file.
  int64_t offset_;
  // The size of the zip archive file.
  int64_t length_;
};

#endif  // CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_COMPRESSOR_ARCHIVE_MINIZIP_H_
