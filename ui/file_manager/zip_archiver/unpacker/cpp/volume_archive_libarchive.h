// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VOLUME_ARCHIVE_LIBARCHIVE_H_
#define VOLUME_ARCHIVE_LIBARCHIVE_H_

#include <string>

#include "archive.h"

#include "volume_archive.h"

// A namespace with constants used by VolumeArchiveLibarchive.
namespace volume_archive_constants {

const char kArchiveReadNewError[] = "Could not allocate archive.";
const char kFileNotFound[] = "File not found for read data request.";
const char kVolumeReaderError[] = "VolumeReader failed to retrieve data.";
const char kArchiveSupportErrorPrefix[] = "Error at support rar/zip format: ";
const char kArchiveOpenErrorPrefix[] = "Error at open archive: ";
const char kArchiveNextHeaderErrorPrefix[] =
    "Error at reading next header for metadata: ";
const char kArchiveReadDataErrorPrefix[] = "Error at reading data: ";
const char kArchiveReadFreeErrorPrefix[] = "Error at archive free: ";

// The size of the buffer used to skip unnecessary data.
// Should be positive and less than size_t maximum.
const int64_t kDummyBufferSize = 512 * 1024;  // 512 KB

// The size of the buffer used by ReadInProgress to decompress data.
// Should be positive and less than size_t maximum.
const int64_t kDecompressBufferSize = 512 * 1024;  // 512 KB.

// The maximum data chunk size for VolumeReader::Read requests.
// Should be positive.
const int64_t kMaximumDataChunkSize = 512 * 1024;  // 512 KB.

// The minimum data chunk size for VolumeReader::Read requests.
// Should be positive.
const int64_t kMinimumDataChunkSize = 32 * 1024;  // 16 KB.

}  // namespace volume_archive_constants

// Defines an implementation of VolumeArchive that wraps all libarchive
// operations.
class VolumeArchiveLibarchive : public VolumeArchive {
 public:
  explicit VolumeArchiveLibarchive(VolumeReader* reader);

  virtual ~VolumeArchiveLibarchive();

  // See volume_archive_interface.h.
  virtual bool Init(const std::string& encoding);

  // See volume_archive_interface.h.
  virtual Result GetNextHeader();
  virtual Result GetNextHeader(const char** path_name,
                               int64_t* size,
                               bool* is_directory,
                               time_t* modification_time);

  // See volume_archive_interface.h.
  virtual bool SeekHeader(int64_t index);

  // See volume_archive_interface.h.
  virtual int64_t ReadData(int64_t offset,
                           int64_t length,
                           const char** buffer);

  // See volume_archive_interface.h.
  virtual void MaybeDecompressAhead();

  // See volume_archive_interface.h.
  virtual bool Cleanup();

  int64_t reader_data_size() const { return reader_data_size_; }

 private:
  // Decompress length bytes of data starting from offset.
  void DecompressData(int64_t offset, int64_t length);

  // The size of the requested data from VolumeReader.
  int64_t reader_data_size_;

  // The libarchive correspondent archive object.
  archive* archive_;

  // The last reached entry with VolumeArchiveLibarchive::GetNextHeader.
  archive_entry* current_archive_entry_;

  // The data offset, which will be offset + length after last read
  // operation, where offset and length are method parameters for
  // VolumeArchiveLibarchive::ReadData. Data offset is used to improve
  // performance for consecutive calls to VolumeArchiveLibarchive::ReadData.
  //
  // Intead of starting the read from the beginning for every
  // VolumeArchiveLibarchive::ReadData, the next call will start
  // from last_read_data_offset_ in case the offset parameter of
  // VolumeArchiveLibarchive::ReadData has the same value as
  // last_read_data_offset_. This avoids decompressing again the bytes at
  // the begninning of the file, which is the average case scenario.
  // But in case the offset parameter is different than last_read_data_offset_,
  // then dummy_buffer_ will be used to ignore unused bytes.
  int64_t last_read_data_offset_;

  // The length of the last VolumeArchiveLibarchive::ReadData. Used for
  // decompress ahead.
  int64_t last_read_data_length_;

  // Dummy buffer for unused data read using VolumeArchiveLibarchive::ReadData.
  // Sometimes VolumeArchiveLibarchive::ReadData can require reading from
  // offsets different from last_read_data_offset_. In this case some bytes
  // must be skipped. Because seeking is not possible inside compressed files,
  // the bytes will be discarded using this buffer.
  char dummy_buffer_[volume_archive_constants::kDummyBufferSize];

  // The address where the decompressed data starting from
  // decompressed_offset_ is stored. It should point to a valid location
  // inside decompressed_data_buffer_. Necesssary in order to NOT throw
  // away unused decompressed bytes as throwing them away would mean in some
  // situations restarting decompressing the file from the beginning.
  char* decompressed_data_;

  // The actual buffer that contains the decompressed data.
  char decompressed_data_buffer_
      [volume_archive_constants::kDecompressBufferSize];

  // The size of valid data starting from decompressed_data_ that is stored
  // inside decompressed_data_buffer_.
  int64_t decompressed_data_size_;

  // True if VolumeArchiveLibarchive::DecompressData failed.
  bool decompressed_error_;
};

#endif  // VOLUME_ARCHIVE_LIBARCHIVE_H_
