// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/data_pack.h"

#include <errno.h>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/ref_counted_memory.h"
#include "base/string_piece.h"

// For details of the file layout, see
// http://dev.chromium.org/developers/design-documents/linuxresourcesandlocalizedstrings

namespace {

// A word is four bytes.
static const size_t kWord = 4;

static const uint32 kFileFormatVersion = 1;
// Length of file header: version and entry count.
static const size_t kHeaderLength = 2 * sizeof(uint32);

struct DataPackEntry {
  uint32 resource_id;
  uint32 file_offset;
  uint32 length;

  static int CompareById(const void* void_key, const void* void_entry) {
    uint32 key = *reinterpret_cast<const uint32*>(void_key);
    const DataPackEntry* entry =
        reinterpret_cast<const DataPackEntry*>(void_entry);
    if (key < entry->resource_id) {
      return -1;
    } else if (key > entry->resource_id) {
      return 1;
    } else {
      return 0;
    }
  }
};

COMPILE_ASSERT(sizeof(DataPackEntry) == 12, size_of_header_must_be_twelve);

// We're crashing when trying to load a pak file on Windows.  Add some error
// codes for logging.
// http://crbug.com/58056
enum LoadErrors {
  INIT_FAILED = 1,
  BAD_VERSION,
  INDEX_TRUNCATED,
  ENTRY_NOT_FOUND,

  LOAD_ERRORS_COUNT,
};

}  // namespace

namespace ui {

// In .cc for MemoryMappedFile dtor.
DataPack::DataPack() : resource_count_(0) {
}
DataPack::~DataPack() {
}

bool DataPack::Load(const FilePath& path) {
  mmap_.reset(new file_util::MemoryMappedFile);
  if (!mmap_->Initialize(path)) {
    DLOG(ERROR) << "Failed to mmap datapack";
    UMA_HISTOGRAM_ENUMERATION("DataPack.Load", INIT_FAILED,
                              LOAD_ERRORS_COUNT);
    return false;
  }

  // Parse the header of the file.
  // First uint32: version; second: resource count.
  const uint32* ptr = reinterpret_cast<const uint32*>(mmap_->data());
  uint32 version = ptr[0];
  if (version != kFileFormatVersion) {
    LOG(ERROR) << "Bad data pack version: got " << version << ", expected "
               << kFileFormatVersion;
    UMA_HISTOGRAM_ENUMERATION("DataPack.Load", BAD_VERSION,
                              LOAD_ERRORS_COUNT);
    mmap_.reset();
    return false;
  }
  resource_count_ = ptr[1];

  // Sanity check the file.
  // 1) Check we have enough entries.
  if (kHeaderLength + resource_count_ * sizeof(DataPackEntry) >
      mmap_->length()) {
    LOG(ERROR) << "Data pack file corruption: too short for number of "
                  "entries specified.";
    UMA_HISTOGRAM_ENUMERATION("DataPack.Load", INDEX_TRUNCATED,
                              LOAD_ERRORS_COUNT);
    mmap_.reset();
    return false;
  }
  // 2) Verify the entries are within the appropriate bounds.
  for (size_t i = 0; i < resource_count_; ++i) {
    const DataPackEntry* entry = reinterpret_cast<const DataPackEntry*>(
        mmap_->data() + kHeaderLength + (i * sizeof(DataPackEntry)));
    if (entry->file_offset + entry->length > mmap_->length()) {
      LOG(ERROR) << "Entry #" << i << " in data pack points off end of file. "
                 << "Was the file corrupted?";
      UMA_HISTOGRAM_ENUMERATION("DataPack.Load", ENTRY_NOT_FOUND,
                                LOAD_ERRORS_COUNT);
      mmap_.reset();
      return false;
    }
  }

  return true;
}

bool DataPack::GetStringPiece(uint32 resource_id,
                              base::StringPiece* data) const {
  // It won't be hard to make this endian-agnostic, but it's not worth
  // bothering to do right now.
#if defined(__BYTE_ORDER)
  // Linux check
  COMPILE_ASSERT(__BYTE_ORDER == __LITTLE_ENDIAN,
                 datapack_assumes_little_endian);
#elif defined(__BIG_ENDIAN__)
  // Mac check
  #error DataPack assumes little endian
#endif

  DataPackEntry* target = reinterpret_cast<DataPackEntry*>(
      bsearch(&resource_id, mmap_->data() + kHeaderLength, resource_count_,
              sizeof(DataPackEntry), DataPackEntry::CompareById));
  if (!target) {
    return false;
  }

  data->set(mmap_->data() + target->file_offset, target->length);
  return true;
}

RefCountedStaticMemory* DataPack::GetStaticMemory(uint32 resource_id) const {
  base::StringPiece piece;
  if (!GetStringPiece(resource_id, &piece))
    return NULL;

  return new RefCountedStaticMemory(
      reinterpret_cast<const unsigned char*>(piece.data()), piece.length());
}

// static
bool DataPack::WritePack(const FilePath& path,
                         const std::map<uint32, base::StringPiece>& resources) {
  FILE* file = file_util::OpenFile(path, "wb");
  if (!file)
    return false;

  if (fwrite(&kFileFormatVersion, 1, kWord, file) != kWord) {
    LOG(ERROR) << "Failed to write file version";
    file_util::CloseFile(file);
    return false;
  }

  // Note: the python version of this function explicitly sorted keys, but
  // std::map is a sorted associative container, we shouldn't have to do that.
  uint32 entry_count = resources.size();
  if (fwrite(&entry_count, 1, kWord, file) != kWord) {
    LOG(ERROR) << "Failed to write entry count";
    file_util::CloseFile(file);
    return false;
  }

  // Each entry is 3 uint32s.
  uint32 index_length = entry_count * 3 * kWord;
  uint32 data_offset = kHeaderLength + index_length;
  for (std::map<uint32, base::StringPiece>::const_iterator it =
           resources.begin();
       it != resources.end(); ++it) {
    if (fwrite(&it->first, 1, kWord, file) != kWord) {
      LOG(ERROR) << "Failed to write id for " << it->first;
      file_util::CloseFile(file);
      return false;
    }

    if (fwrite(&data_offset, 1, kWord, file) != kWord) {
      LOG(ERROR) << "Failed to write offset for " << it->first;
      file_util::CloseFile(file);
      return false;
    }

    uint32 len = it->second.length();
    if (fwrite(&len, 1, kWord, file) != kWord) {
      LOG(ERROR) << "Failed to write length for " << it->first;
      file_util::CloseFile(file);
      return false;
    }

    data_offset += len;
  }

  for (std::map<uint32, base::StringPiece>::const_iterator it =
           resources.begin();
       it != resources.end(); ++it) {
    if (fwrite(it->second.data(), it->second.length(), 1, file) != 1) {
      LOG(ERROR) << "Failed to write data for " << it->first;
      file_util::CloseFile(file);
      return false;
    }
  }

  file_util::CloseFile(file);

  return true;
}

}  // namespace ui
