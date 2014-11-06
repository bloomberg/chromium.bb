// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_zip.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "crazy_linker_debug.h"
#include "crazy_linker_system.h"
#include "crazy_linker_util.h"

namespace {

// All offsets are given in bytes relative to the start of the header.
// Arithmetic is used to indicate the size of small fields that are skipped.

// http://www.pkware.com/documents/casestudies/APPNOTE.TXT Section 4.3.16
// This marker appears at the start of the end of central directory record
const uint32_t kEndOfCentralDirectoryMarker = 0x06054b50;

// Offsets of fields in End of Central Directory.
const int kOffsetNumOfEntriesInEndOfCentralDirectory = 4 + 2 + 2;
const int kOffsetOfCentralDirLengthInEndOfCentralDirectory =
    kOffsetNumOfEntriesInEndOfCentralDirectory + 2 + 2;
const int kOffsetOfStartOfCentralDirInEndOfCentralDirectory =
    kOffsetOfCentralDirLengthInEndOfCentralDirectory + 4;

// http://www.pkware.com/documents/casestudies/APPNOTE.TXT Section 4.3.12
// This marker appears at the start of the central directory
const uint32_t kCentralDirHeaderMarker = 0x2014b50;

// Offsets of fields in Central Directory Header.
const int kOffsetFilenameLengthInCentralDirectory =
    4 + 2 + 2 + 2 + 2 + 2 + 2 + 4 + 4 + 4;
const int kOffsetExtraFieldLengthInCentralDirectory =
    kOffsetFilenameLengthInCentralDirectory + 2;
const int kOffsetCommentLengthInCentralDirectory =
    kOffsetExtraFieldLengthInCentralDirectory + 2;
const int kOffsetLocalHeaderOffsetInCentralDirectory =
    kOffsetCommentLengthInCentralDirectory + 2 + 2 + 2 + 4;
const int kOffsetFilenameInCentralDirectory =
    kOffsetLocalHeaderOffsetInCentralDirectory + 4;

// http://www.pkware.com/documents/casestudies/APPNOTE.TXT Section 4.3.7
// This marker appears at the start of local header
const uint32_t kLocalHeaderMarker = 0x04034b50;

// http://www.pkware.com/documents/casestudies/APPNOTE.TXT Section 4.4.5
// This value denotes that the file is stored (no compression).
const uint32_t kCompressionMethodStored = 0;

// Offsets of fields in the Local Header.
const int kOffsetCompressionMethodInLocalHeader = 4 + 2 + 2;
const int kOffsetFilenameLengthInLocalHeader =
    kOffsetCompressionMethodInLocalHeader + 2 + 2 + 2 + 4 + 4 + 4;
const int kOffsetExtraFieldLengthInLocalHeader =
    kOffsetFilenameLengthInLocalHeader + 2;
const int kOffsetFilenameInLocalHeader =
    kOffsetExtraFieldLengthInLocalHeader + 2;

// RAII pattern for unmapping and closing the mapped file.
class ScopedMMap {
 public:
  ScopedMMap(void* mem, uint32_t len) : mem_(mem), len_(len) {}
  ~ScopedMMap() {
    if (munmap(mem_, len_) == -1) {
      LOG_ERRNO("%s: munmap failed when trying to unmap zip file\n",
                __FUNCTION__);
    }
  }
 private:
  void* mem_;
  uint32_t len_;
};

inline uint32_t ReadUInt16(uint8_t* mem_bytes, int offset) {
  return
      static_cast<uint32_t>(mem_bytes[offset]) |
      (static_cast<uint32_t>(mem_bytes[offset + 1]) << 8);
}

inline uint32_t ReadUInt32(uint8_t* mem_bytes, int offset) {
  return
      static_cast<uint32_t>(mem_bytes[offset]) |
      (static_cast<uint32_t>(mem_bytes[offset + 1]) << 8) |
      (static_cast<uint32_t>(mem_bytes[offset + 2]) << 16) |
      (static_cast<uint32_t>(mem_bytes[offset + 3]) << 24);
}

}  // unnamed namespace

namespace crazy {

const uint32_t kMaxZipFileLength = 1U << 31;  // 2GB

int FindStartOffsetOfFileInZipFile(const char* zip_file, const char* filename) {
  // Open the file
  FileDescriptor fd;
  if (!fd.OpenReadOnly(zip_file)) {
    LOG_ERRNO("%s: open failed trying to open zip file %s\n",
              __FUNCTION__, zip_file);
    return CRAZY_OFFSET_FAILED;
  }

  // Find the length of the file.
  struct stat stat_buf;
  if (stat(zip_file, &stat_buf) == -1) {
    LOG_ERRNO("%s: stat failed trying to stat zip file %s\n",
              __FUNCTION__, zip_file);
    return CRAZY_OFFSET_FAILED;
  }

  if (stat_buf.st_size > kMaxZipFileLength) {
    LOG("%s: The size %ld of %s is too large to map\n",
        __FUNCTION__, stat_buf.st_size, zip_file);
    return CRAZY_OFFSET_FAILED;
  }

  // Map the file into memory.
  void* mem = fd.Map(NULL, stat_buf.st_size, PROT_READ, MAP_PRIVATE, 0);
  if (mem == MAP_FAILED) {
    LOG_ERRNO("%s: mmap failed trying to mmap zip file %s\n",
              __FUNCTION__, zip_file);
    return CRAZY_OFFSET_FAILED;
  }
  ScopedMMap scoped_mmap(mem, stat_buf.st_size);

  // Scan backwards from the end of the file searching for the end of
  // central directory marker.
  uint8_t* mem_bytes = static_cast<uint8_t*>(mem);
  int off;
  for (off = stat_buf.st_size - sizeof(kEndOfCentralDirectoryMarker);
       off >= 0; --off) {
    if (ReadUInt32(mem_bytes, off) == kEndOfCentralDirectoryMarker) {
      break;
    }
  }
  if (off == -1) {
    LOG("%s: Failed to find end of central directory in %s\n",
        __FUNCTION__, zip_file);
    return CRAZY_OFFSET_FAILED;
  }

  // We have located the end of central directory record, now locate
  // the central directory by reading the end of central directory record.

  uint32_t length_of_central_dir = ReadUInt32(
      mem_bytes, off + kOffsetOfCentralDirLengthInEndOfCentralDirectory);
  uint32_t start_of_central_dir = ReadUInt32(
      mem_bytes, off + kOffsetOfStartOfCentralDirInEndOfCentralDirectory);

  if (start_of_central_dir > off) {
    LOG("%s: Found out of range offset %u for start of directory in %s\n",
        __FUNCTION__, start_of_central_dir, zip_file);
    return CRAZY_OFFSET_FAILED;
  }

  uint32_t end_of_central_dir = start_of_central_dir + length_of_central_dir;
  if (end_of_central_dir > off) {
    LOG("%s: Found out of range offset %u for end of directory in %s\n",
        __FUNCTION__, end_of_central_dir, zip_file);
    return CRAZY_OFFSET_FAILED;
  }

  uint32_t num_entries = ReadUInt16(
      mem_bytes, off + kOffsetNumOfEntriesInEndOfCentralDirectory);

  // Read the headers in the central directory and locate the file.
  off = start_of_central_dir;
  const int target_len = strlen(filename);
  int n = 0;
  for (; n < num_entries && off < end_of_central_dir; ++n) {
    uint32_t marker = ReadUInt32(mem_bytes, off);
    if (marker != kCentralDirHeaderMarker) {
      LOG("%s: Failed to find central directory header marker in %s. "
          "Found 0x%x but expected 0x%x\n", __FUNCTION__,
          zip_file, marker, kCentralDirHeaderMarker);
      return CRAZY_OFFSET_FAILED;
    }
    uint32_t file_name_length =
        ReadUInt16(mem_bytes, off + kOffsetFilenameLengthInCentralDirectory);
    uint32_t extra_field_length =
        ReadUInt16(mem_bytes, off + kOffsetExtraFieldLengthInCentralDirectory);
    uint32_t comment_field_length =
        ReadUInt16(mem_bytes, off + kOffsetCommentLengthInCentralDirectory);
    uint32_t header_length = kOffsetFilenameInCentralDirectory +
        file_name_length + extra_field_length + comment_field_length;

    uint32_t local_header_offset =
        ReadUInt32(mem_bytes, off + kOffsetLocalHeaderOffsetInCentralDirectory);

    uint8_t* filename_bytes =
        mem_bytes + off + kOffsetFilenameInCentralDirectory;

    if (file_name_length == target_len &&
        memcmp(filename_bytes, filename, target_len) == 0) {
      // Filename matches. Read the local header and compute the offset.
      uint32_t marker = ReadUInt32(mem_bytes, local_header_offset);
      if (marker != kLocalHeaderMarker) {
        LOG("%s: Failed to find local file header marker in %s. "
            "Found 0x%x but expected 0x%x\n", __FUNCTION__,
            zip_file, marker, kLocalHeaderMarker);
        return CRAZY_OFFSET_FAILED;
      }

      uint32_t compression_method =
          ReadUInt16(
              mem_bytes,
              local_header_offset + kOffsetCompressionMethodInLocalHeader);
      if (compression_method != kCompressionMethodStored) {
        LOG("%s: %s is compressed within %s. "
            "Found compression method %u but expected %u\n", __FUNCTION__,
            filename, zip_file, compression_method, kCompressionMethodStored);
        return CRAZY_OFFSET_FAILED;
      }

      uint32_t file_name_length =
          ReadUInt16(
              mem_bytes,
              local_header_offset + kOffsetFilenameLengthInLocalHeader);
      uint32_t extra_field_length =
          ReadUInt16(
              mem_bytes,
              local_header_offset + kOffsetExtraFieldLengthInLocalHeader);
      uint32_t header_length =
          kOffsetFilenameInLocalHeader + file_name_length + extra_field_length;

      return local_header_offset + header_length;
    }

    off += header_length;
  }

  if (n < num_entries) {
    LOG("%s: Did not find all the expected entries in the central directory. "
        "Found %d but expected %d\n", __FUNCTION__, n, num_entries);
  }

  if (off < end_of_central_dir) {
    LOG("%s: There are %d extra bytes at the end of the central directory.\n",
        __FUNCTION__, end_of_central_dir - off);
  }

  LOG("%s: Did not find %s in %s\n", __FUNCTION__, filename, zip_file);
  return CRAZY_OFFSET_FAILED;
}

}  // crazy namespace
