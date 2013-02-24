// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_usage_cache.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/pickle.h"

namespace fileapi {

const base::FilePath::CharType FileSystemUsageCache::kUsageFileName[] =
    FILE_PATH_LITERAL(".usage");
const char FileSystemUsageCache::kUsageFileHeader[] = "FSU5";
const int FileSystemUsageCache::kUsageFileHeaderSize = 4;

/* Pickle::{Read,Write}Bool treat bool as int */
const int FileSystemUsageCache::kUsageFileSize =
    sizeof(Pickle::Header) +
    FileSystemUsageCache::kUsageFileHeaderSize +
    sizeof(int) + sizeof(int32) + sizeof(int64);

// static
int64 FileSystemUsageCache::GetUsage(const base::FilePath& usage_file_path) {
  bool is_valid = true;
  uint32 dirty = 0;
  int64 fs_usage;
  fs_usage = Read(usage_file_path, &is_valid, &dirty);

  if (fs_usage < 0)
    return -1;

  return fs_usage;
}

// static
int32 FileSystemUsageCache::GetDirty(const base::FilePath& usage_file_path) {
  bool is_valid = true;
  uint32 dirty = 0;
  int64 fs_usage;
  fs_usage = Read(usage_file_path, &is_valid, &dirty);

  if (fs_usage < 0)
    return -1;

  return static_cast<int32>(dirty);
}

// static
bool FileSystemUsageCache::IncrementDirty(const base::FilePath& usage_file_path) {
  bool is_valid = true;
  uint32 dirty = 0;
  int64 fs_usage;
  fs_usage = Read(usage_file_path, &is_valid, &dirty);

  if (fs_usage < 0)
    return false;

  return Write(usage_file_path, is_valid, dirty + 1, fs_usage) >= 0;
}

// static
bool FileSystemUsageCache::DecrementDirty(const base::FilePath& usage_file_path) {
  bool is_valid = true;
  uint32 dirty = 0;
  int64 fs_usage;
  fs_usage = Read(usage_file_path, &is_valid, &dirty);

  if (fs_usage < 0 || dirty <= 0)
    return false;

  return Write(usage_file_path, is_valid, dirty - 1, fs_usage) >= 0;
}

// static
bool FileSystemUsageCache::Invalidate(const base::FilePath& usage_file_path) {
  bool is_valid = true;
  uint32 dirty = 0;
  int64 fs_usage;
  fs_usage = Read(usage_file_path, &is_valid, &dirty);

  return fs_usage >= 0 && Write(usage_file_path, false, dirty, fs_usage);
}

bool FileSystemUsageCache::IsValid(const base::FilePath& usage_file_path) {
  bool is_valid = true;
  uint32 dirty = 0;
  int64 result = Read(usage_file_path, &is_valid, &dirty);
  if (result < 0)
    return false;

  return is_valid;
}

// static
int FileSystemUsageCache::AtomicUpdateUsageByDelta(
    const base::FilePath& usage_file_path, int64 delta) {
  bool is_valid = true;
  uint32 dirty = 0;
  int64 fs_usage;
  // TODO(dmikurube): Make sure that usage_file_path is available.
  fs_usage = Read(usage_file_path, &is_valid, &dirty);

  if (fs_usage < 0)
    return -1;

  return Write(usage_file_path, is_valid, dirty, fs_usage + delta);
}

// static
int FileSystemUsageCache::UpdateUsage(const base::FilePath& usage_file_path,
                                      int64 fs_usage) {
  return Write(usage_file_path, true, 0, fs_usage);
}

// static
bool FileSystemUsageCache::Exists(const base::FilePath& usage_file_path) {
  return file_util::PathExists(usage_file_path);
}

// static
bool FileSystemUsageCache::Delete(const base::FilePath& usage_file_path) {
  return file_util::Delete(usage_file_path, true);
}

// static
int64 FileSystemUsageCache::Read(const base::FilePath& usage_file_path,
                                 bool* is_valid,
                                 uint32* dirty) {
  char buffer[kUsageFileSize];
  const char *header;
  if (usage_file_path.empty() ||
      kUsageFileSize != file_util::ReadFile(usage_file_path,
                                            buffer, kUsageFileSize))
    return -1;
  Pickle read_pickle(buffer, kUsageFileSize);
  PickleIterator iter(read_pickle);
  int64 fs_usage;

  if (!read_pickle.ReadBytes(&iter, &header, kUsageFileHeaderSize) ||
      !read_pickle.ReadBool(&iter, is_valid) ||
      !read_pickle.ReadUInt32(&iter, dirty) ||
      !read_pickle.ReadInt64(&iter, &fs_usage))
    return -1;

  if (header[0] != kUsageFileHeader[0] ||
      header[1] != kUsageFileHeader[1] ||
      header[2] != kUsageFileHeader[2] ||
      header[3] != kUsageFileHeader[3])
    return -1;

  return fs_usage;
}

// static
int FileSystemUsageCache::Write(const base::FilePath& usage_file_path,
                                bool is_valid,
                                uint32 dirty,
                                int64 fs_usage) {
  Pickle write_pickle;
  write_pickle.WriteBytes(kUsageFileHeader, kUsageFileHeaderSize);
  write_pickle.WriteBool(is_valid);
  write_pickle.WriteUInt32(dirty);
  write_pickle.WriteInt64(fs_usage);

  base::FilePath temporary_usage_file_path;
  if (usage_file_path.empty() ||
      !file_util::CreateTemporaryFileInDir(usage_file_path.DirName(),
                                           &temporary_usage_file_path)) {
    return -1;
  }

  int bytes_written = file_util::WriteFile(temporary_usage_file_path,
                                           (const char *)write_pickle.data(),
                                           write_pickle.size());
  if (bytes_written != kUsageFileSize)
    return -1;

  if (file_util::ReplaceFile(temporary_usage_file_path, usage_file_path))
    return bytes_written;
  else
    return -1;
}

}  // namespace fileapi
