// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_usage_cache.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/pickle.h"

namespace fileapi {

const char FileSystemUsageCache::kUsageFileName[] = ".usage";
const char FileSystemUsageCache::kUsageFileHeader[] = "FSU0";
const int FileSystemUsageCache::kUsageFileHeaderSize = 4;
const int FileSystemUsageCache::kUsageFileSize =
    sizeof(Pickle::Header) +
    FileSystemUsageCache::kUsageFileHeaderSize +
    sizeof(int32) + sizeof(int64);

// static
int64 FileSystemUsageCache::GetUsage(const FilePath& usage_file_path) {
  uint32 dirty = 0;
  int64 fs_usage;
  fs_usage = Read(usage_file_path, &dirty);

  if (fs_usage < 0 || dirty > 0)
    return -1;

  return fs_usage;
}

// static
bool FileSystemUsageCache::IncrementDirty(const FilePath& usage_file_path) {
  uint32 dirty = 0;
  int64 fs_usage;
  fs_usage = Read(usage_file_path, &dirty);

  if (fs_usage < 0)
    return false;

  return Write(usage_file_path, dirty + 1, fs_usage) >= 0;
}

// static
bool FileSystemUsageCache::DecrementDirty(const FilePath& usage_file_path) {
  uint32 dirty = 0;
  int64 fs_usage;
  fs_usage = Read(usage_file_path, &dirty);

  if (fs_usage < 0 || dirty <= 0)
    return false;

  return Write(usage_file_path, dirty - 1, fs_usage) >= 0;
}

// static
int FileSystemUsageCache::UpdateUsage(const FilePath& usage_file_path,
                                      int64 fs_usage) {
  return Write(usage_file_path, 0, fs_usage);
}

// static
bool FileSystemUsageCache::Exists(const FilePath& usage_file_path) {
  return file_util::PathExists(usage_file_path);
}

// static
bool FileSystemUsageCache::Delete(const FilePath& usage_file_path) {
  return file_util::Delete(usage_file_path, true);
}

// static
int64 FileSystemUsageCache::Read(const FilePath& usage_file_path,
                                 uint32* dirty) {
  char buffer[kUsageFileSize];
  const char *header;
  file_util::ReadFile(usage_file_path, buffer, kUsageFileSize);
  Pickle read_pickle(buffer, kUsageFileSize);
  void* iter = NULL;
  int64 fs_usage;

  if (!read_pickle.ReadBytes(&iter, &header, kUsageFileHeaderSize) ||
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
int FileSystemUsageCache::Write(const FilePath& usage_file_path,
                                uint32 dirty, int64 fs_usage) {
  Pickle write_pickle;
  write_pickle.WriteBytes(kUsageFileHeader, kUsageFileHeaderSize);
  write_pickle.WriteUInt32(dirty);
  write_pickle.WriteInt64(fs_usage);

  FilePath temporary_usage_file_path;
  file_util::CreateTemporaryFileInDir(usage_file_path.DirName(),
                                      &temporary_usage_file_path);
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

}
