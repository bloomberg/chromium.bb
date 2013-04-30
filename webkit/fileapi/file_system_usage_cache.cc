// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_usage_cache.h"

#include <utility>

#include "base/debug/trace_event.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/pickle.h"
#include "base/stl_util.h"

namespace fileapi {

namespace {
const int64 kCloseDelaySeconds = 5;
const size_t kMaxHandleCacheSize = 2;
}  // namespace

FileSystemUsageCache::FileSystemUsageCache(
    base::SequencedTaskRunner* task_runner)
    : weak_factory_(this),
      task_runner_(task_runner) {
}

FileSystemUsageCache::~FileSystemUsageCache() {
  task_runner_ = NULL;
  CloseCacheFiles();
}

const base::FilePath::CharType FileSystemUsageCache::kUsageFileName[] =
    FILE_PATH_LITERAL(".usage");
const char FileSystemUsageCache::kUsageFileHeader[] = "FSU5";
const int FileSystemUsageCache::kUsageFileHeaderSize = 4;

// Pickle::{Read,Write}Bool treat bool as int
const int FileSystemUsageCache::kUsageFileSize =
    sizeof(Pickle::Header) +
    FileSystemUsageCache::kUsageFileHeaderSize +
    sizeof(int) + sizeof(int32) + sizeof(int64);  // NOLINT

bool FileSystemUsageCache::GetUsage(const base::FilePath& usage_file_path,
                                    int64* usage_out) {
  TRACE_EVENT0("FileSystem", "UsageCache::GetUsage");
  DCHECK(CalledOnValidThread());
  DCHECK(usage_out);
  bool is_valid = true;
  uint32 dirty = 0;
  int64 usage = 0;
  if (!Read(usage_file_path, &is_valid, &dirty, &usage))
    return false;
  *usage_out = usage;
  return true;
}

bool FileSystemUsageCache::GetDirty(const base::FilePath& usage_file_path,
                                    uint32* dirty_out) {
  TRACE_EVENT0("FileSystem", "UsageCache::GetDirty");
  DCHECK(CalledOnValidThread());
  DCHECK(dirty_out);
  bool is_valid = true;
  uint32 dirty = 0;
  int64 usage = 0;
  if (!Read(usage_file_path, &is_valid, &dirty, &usage))
    return false;
  *dirty_out = dirty;
  return true;
}

bool FileSystemUsageCache::IncrementDirty(
    const base::FilePath& usage_file_path) {
  TRACE_EVENT0("FileSystem", "UsageCache::IncrementDirty");
  DCHECK(CalledOnValidThread());
  bool is_valid = true;
  uint32 dirty = 0;
  int64 usage = 0;
  bool new_handle = !HasCacheFileHandle(usage_file_path);
  if (!Read(usage_file_path, &is_valid, &dirty, &usage))
    return false;

  bool success = Write(usage_file_path, is_valid, dirty + 1, usage);
  if (success && dirty == 0 && new_handle)
    FlushFile(usage_file_path);
  return success;
}

bool FileSystemUsageCache::DecrementDirty(
    const base::FilePath& usage_file_path) {
  TRACE_EVENT0("FileSystem", "UsageCache::DecrementDirty");
  DCHECK(CalledOnValidThread());
  bool is_valid = true;
  uint32 dirty = 0;
  int64 usage = 0;;
  if (!Read(usage_file_path, &is_valid, &dirty, &usage) || dirty <= 0)
    return false;

  if (dirty <= 0)
    return false;

  return Write(usage_file_path, is_valid, dirty - 1, usage);
}

bool FileSystemUsageCache::Invalidate(const base::FilePath& usage_file_path) {
  TRACE_EVENT0("FileSystem", "UsageCache::Invalidate");
  DCHECK(CalledOnValidThread());
  bool is_valid = true;
  uint32 dirty = 0;
  int64 usage = 0;
  if (!Read(usage_file_path, &is_valid, &dirty, &usage))
    return false;

  return Write(usage_file_path, false, dirty, usage);
}

bool FileSystemUsageCache::IsValid(const base::FilePath& usage_file_path) {
  TRACE_EVENT0("FileSystem", "UsageCache::IsValid");
  DCHECK(CalledOnValidThread());
  bool is_valid = true;
  uint32 dirty = 0;
  int64 usage = 0;
  if (!Read(usage_file_path, &is_valid, &dirty, &usage))
    return false;
  return is_valid;
}

bool FileSystemUsageCache::AtomicUpdateUsageByDelta(
    const base::FilePath& usage_file_path, int64 delta) {
  TRACE_EVENT0("FileSystem", "UsageCache::AtomicUpdateUsageByDelta");
  DCHECK(CalledOnValidThread());
  bool is_valid = true;
  uint32 dirty = 0;
  int64 usage = 0;;
  if (!Read(usage_file_path, &is_valid, &dirty, &usage))
    return false;
  return Write(usage_file_path, is_valid, dirty, usage + delta);
}

bool FileSystemUsageCache::UpdateUsage(const base::FilePath& usage_file_path,
                                       int64 fs_usage) {
  TRACE_EVENT0("FileSystem", "UsageCache::UpdateUsage");
  DCHECK(CalledOnValidThread());
  return Write(usage_file_path, true, 0, fs_usage);
}

bool FileSystemUsageCache::Exists(const base::FilePath& usage_file_path) {
  TRACE_EVENT0("FileSystem", "UsageCache::Exists");
  DCHECK(CalledOnValidThread());
  return file_util::PathExists(usage_file_path);
}

bool FileSystemUsageCache::Delete(const base::FilePath& usage_file_path) {
  TRACE_EVENT0("FileSystem", "UsageCache::Delete");
  DCHECK(CalledOnValidThread());
  CloseCacheFiles();
  return file_util::Delete(usage_file_path, true);
}

void FileSystemUsageCache::CloseCacheFiles() {
  TRACE_EVENT0("FileSystem", "UsageCache::CloseCacheFiles");
  DCHECK(CalledOnValidThread());
  for (CacheFiles::iterator itr = cache_files_.begin();
       itr != cache_files_.end(); ++itr) {
    if (itr->second != base::kInvalidPlatformFileValue)
      base::ClosePlatformFile(itr->second);
  }
  cache_files_.clear();
  timer_.Stop();
}

bool FileSystemUsageCache::Read(const base::FilePath& usage_file_path,
                                 bool* is_valid,
                                 uint32* dirty_out,
                                 int64* usage_out) {
  TRACE_EVENT0("FileSystem", "UsageCache::Read");
  DCHECK(CalledOnValidThread());
  DCHECK(is_valid);
  DCHECK(dirty_out);
  DCHECK(usage_out);
  char buffer[kUsageFileSize];
  const char *header;
  if (usage_file_path.empty() ||
      !ReadBytes(usage_file_path, buffer, kUsageFileSize))
    return false;
  Pickle read_pickle(buffer, kUsageFileSize);
  PickleIterator iter(read_pickle);
  uint32 dirty = 0;
  int64 usage = 0;

  if (!read_pickle.ReadBytes(&iter, &header, kUsageFileHeaderSize) ||
      !read_pickle.ReadBool(&iter, is_valid) ||
      !read_pickle.ReadUInt32(&iter, &dirty) ||
      !read_pickle.ReadInt64(&iter, &usage))
    return false;

  if (header[0] != kUsageFileHeader[0] ||
      header[1] != kUsageFileHeader[1] ||
      header[2] != kUsageFileHeader[2] ||
      header[3] != kUsageFileHeader[3])
    return false;

  *dirty_out = dirty;
  *usage_out = usage;
  return true;
}

bool FileSystemUsageCache::Write(const base::FilePath& usage_file_path,
                                 bool is_valid,
                                 int32 dirty,
                                 int64 usage) {
  TRACE_EVENT0("FileSystem", "UsageCache::Write");
  DCHECK(CalledOnValidThread());
  Pickle write_pickle;
  write_pickle.WriteBytes(kUsageFileHeader, kUsageFileHeaderSize);
  write_pickle.WriteBool(is_valid);
  write_pickle.WriteUInt32(dirty);
  write_pickle.WriteInt64(usage);

  if (!WriteBytes(usage_file_path,
                  static_cast<const char*>(write_pickle.data()),
                  write_pickle.size())) {
    Delete(usage_file_path);
    return false;
  }
  return true;
}

bool FileSystemUsageCache::GetPlatformFile(const base::FilePath& file_path,
                                           base::PlatformFile* file) {
  DCHECK(CalledOnValidThread());
  if (cache_files_.size() >= kMaxHandleCacheSize)
    CloseCacheFiles();
  ScheduleCloseTimer();

  std::pair<CacheFiles::iterator, bool> inserted =
      cache_files_.insert(
          std::make_pair(file_path, base::kInvalidPlatformFileValue));
  if (!inserted.second) {
    *file = inserted.first->second;
    return true;
  }

  base::PlatformFileError error = base::PLATFORM_FILE_ERROR_FAILED;
  base::PlatformFile platform_file =
      base::CreatePlatformFile(file_path,
                               base::PLATFORM_FILE_OPEN_ALWAYS |
                               base::PLATFORM_FILE_READ |
                               base::PLATFORM_FILE_WRITE,
                               NULL, &error);
  if (error != base::PLATFORM_FILE_OK) {
    cache_files_.erase(inserted.first);
    return false;
  }

  inserted.first->second = platform_file;
  *file = platform_file;
  return true;
}

bool FileSystemUsageCache::ReadBytes(const base::FilePath& file_path,
                                     char* buffer,
                                     int64 buffer_size) {
  DCHECK(CalledOnValidThread());
  base::PlatformFile file;
  if (!GetPlatformFile(file_path, &file))
    return false;
  return base::ReadPlatformFile(file, 0, buffer, buffer_size) == buffer_size;
}

bool FileSystemUsageCache::WriteBytes(const base::FilePath& file_path,
                                      const char* buffer,
                                      int64 buffer_size) {
  DCHECK(CalledOnValidThread());
  base::PlatformFile file;
  if (!GetPlatformFile(file_path, &file))
    return false;
  return base::WritePlatformFile(file, 0, buffer, buffer_size) == buffer_size;
}

bool FileSystemUsageCache::FlushFile(const base::FilePath& file_path) {
  TRACE_EVENT0("FileSystem", "UsageCache::FlushFile");
  DCHECK(CalledOnValidThread());
  base::PlatformFile file = base::kInvalidPlatformFileValue;
  return GetPlatformFile(file_path, &file) && base::FlushPlatformFile(file);
}

void FileSystemUsageCache::ScheduleCloseTimer() {
  DCHECK(CalledOnValidThread());
  if (timer_.IsRunning()) {
    timer_.Reset();
    return;
  }

  timer_.Start(FROM_HERE,
               base::TimeDelta::FromSeconds(kCloseDelaySeconds),
               base::Bind(&FileSystemUsageCache::CloseCacheFiles,
                          weak_factory_.GetWeakPtr()));
}

bool FileSystemUsageCache::CalledOnValidThread() {
  return !task_runner_ || task_runner_->RunsTasksOnCurrentThread();
}

bool FileSystemUsageCache::HasCacheFileHandle(const base::FilePath& file_path) {
  DCHECK(CalledOnValidThread());
  DCHECK_LE(cache_files_.size(), kMaxHandleCacheSize);
  return ContainsKey(cache_files_, file_path);
}

}  // namespace fileapi
