// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_COMPRESSOR_ARCHIVE_MINIZIP_H_
#define CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_COMPRESSOR_ARCHIVE_MINIZIP_H_

#include <memory>
#include <string>

#include "chrome/browser/resources/chromeos/zip_archiver/cpp/compressor_archive.h"
#include "third_party/minizip/src/unzip.h"
#include "third_party/minizip/src/zip.h"

class CompressorStream;

// A name space with custom functions passed to minizip.
namespace compressor_archive_functions {

uLong CustomArchiveWrite(void* compressor,
                         void* stream,
                         const void* buffer,
                         uLong length);

long CustomArchiveTell(void* compressor, void* stream);

long CustomArchiveSeek(void* compressor,
                       void* stream,
                       uLong offset,
                       int origin);

}  // namespace compressor_archive_functions

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
                    int64_t modification_time,
                    bool is_directory) override;

  // A getter function for zip_file_.
  zipFile zip_file() const { return zip_file_; }

  // Getter and setter for offset_.
  int64_t offset() const { return offset_; }
  void set_offset(int64_t value) { offset_ = value; }

  // Getter and setter for length_.
  int64_t length() const { return length_; }
  void set_length(int64_t value) { length_ = value; }

  // A getter function for compressor_stream.
  CompressorStream* compressor_stream() const { return compressor_stream_; }

  // Custom functions need to access private variables of
  // CompressorArchiveMinizip frequently.
  friend uLong compressor_archive_functions::CustomArchiveWrite(
      void* compressor,
      void* stream,
      const void* buffer,
      uLong length);

  friend long compressor_archive_functions::CustomArchiveTell(void* compressor,
                                                              void* stream);

  friend long compressor_archive_functions::CustomArchiveSeek(void* compressor,
                                                              void* stream,
                                                              uLong offset,
                                                              int origin);

 private:
  // An instance that takes care of all IO operations.
  CompressorStream* compressor_stream_;

  // The minizip correspondent archive object.
  zipFile zip_file_;

  // The buffer used to store the data read from JavaScript.
  std::unique_ptr<char[]> destination_buffer_;

  // The current offset of the zip archive file.
  int64_t offset_;
  // The size of the zip archive file.
  int64_t length_;
};

#endif  // CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_COMPRESSOR_ARCHIVE_MINIZIP_H_
