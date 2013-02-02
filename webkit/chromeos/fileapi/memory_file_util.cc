// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "webkit/chromeos/fileapi/memory_file_util.h"

namespace {
const int kDefaultReadDirectoryBufferSize = 100;
}  // namespace

namespace fileapi {

// In-memory implementation of AsyncFileStream.
class MemoryFileUtilAsyncFileStream : public AsyncFileStream {
 public:
  // |file_entry| is owned by MemoryFileUtil.
  MemoryFileUtilAsyncFileStream(MemoryFileUtil::FileEntry* file_entry,
                                int flags)
      : file_entry_(file_entry),
        flags_(flags),
        offset_(0),
        weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  }

  // AsyncFileStream override.
  virtual void Read(char* buffer,
                    int64 length,
                    const ReadWriteCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&MemoryFileUtilAsyncFileStream::DoRead,
                   weak_ptr_factory_.GetWeakPtr(),
                   buffer, length, callback));
  }

  // AsyncFileStream override.
  virtual void Write(const char* buffer,
                     int64 length,
                     const ReadWriteCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&MemoryFileUtilAsyncFileStream::DoWrite,
                   weak_ptr_factory_.GetWeakPtr(),
                   buffer, length, callback));
  }

  // AsyncFileStream override.
  virtual void Seek(int64 offset,
                    const SeekCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&MemoryFileUtilAsyncFileStream::DoSeek,
                   weak_ptr_factory_.GetWeakPtr(),
                   offset, callback));
  }

 private:
  // Callback used for Read().
  void DoRead(char* buffer,
              int64 length,
              const ReadWriteCallback& callback) {

    if ((flags_ & base::PLATFORM_FILE_READ) == 0) {
      callback.Run(base::PLATFORM_FILE_ERROR_INVALID_OPERATION, 0);
      return;
    }

    // Shorten the length so the read does not overrun.
    length = std::min(length, file_size() - offset_);

    const std::string& contents = file_entry_->contents;
    std::copy(contents.begin() + offset_,
              contents.begin() + offset_ + length,
              buffer);
    offset_ += length;

    callback.Run(base::PLATFORM_FILE_OK, length);
  }

  // Callback used for Write().
  void DoWrite(const char* buffer,
               int64 length,
               const ReadWriteCallback& callback) {
    if ((flags_ & base::PLATFORM_FILE_WRITE) == 0) {
      callback.Run(base::PLATFORM_FILE_ERROR_INVALID_OPERATION, 0);
      return;
    }

    // Extend the contents if needed.
    std::string* contents = &file_entry_->contents;
    if (offset_ + length > file_size())
      contents->resize(offset_ + length, 0);  // Fill with 0.

    std::copy(buffer, buffer + length,
              contents->begin() + offset_);
    file_entry_->last_modified = base::Time::Now();
    offset_ += length;

    callback.Run(base::PLATFORM_FILE_OK, length);
  }

  // Callback used for Seek().
  void DoSeek(int64 offset,
              const SeekCallback& callback) {
    if (offset > file_size()) {
      // Unlike lseek(2), we don't allow an offset larger than the file
      // size for this file implementation.
      callback.Run(base::PLATFORM_FILE_ERROR_INVALID_OPERATION);
      return;
    }

    offset_ = offset;
    callback.Run(base::PLATFORM_FILE_OK);
  }

  // Returns the file size as int64.
  int64 file_size() const {
    return static_cast<int64>(file_entry_->contents.size());
  }

  MemoryFileUtil::FileEntry* file_entry_;
  const int flags_;
  int64 offset_;
  base::WeakPtrFactory<MemoryFileUtilAsyncFileStream> weak_ptr_factory_;
};

MemoryFileUtil::FileEntry::FileEntry()
    : is_directory(false) {
}

MemoryFileUtil::FileEntry::~FileEntry() {
}

MemoryFileUtil::MemoryFileUtil(const base::FilePath& root_path)
    : read_directory_buffer_size_(kDefaultReadDirectoryBufferSize) {
  FileEntry root;
  root.is_directory = true;
  root.last_modified = base::Time::Now();

  files_[root_path] = root;
}

MemoryFileUtil::~MemoryFileUtil() {
}

// Depending on the flags value the flow of file opening will be one of
// the following:
//
// PLATFORM_FILE_OPEN:
// - GetFileInfo
// - DidGetFileInfoForOpen
// - OpenVerifiedFile
//
// PLATFORM_FILE_CREATE:
// - Create
// - DidCreateOrTruncateForOpen
// - OpenVerifiedFile
//
// PLATFORM_FILE_OPEN_ALWAYS:
// - GetFileInfo
// - DidGetFileInfoForOpen
// - OpenVerifiedFile   OR   Create
//                           DidCreateOrTruncateForOpen
//                           OpenVerifiedFile
//
// PLATFORM_FILE_CREATE_ALWAYS:
// - Truncate
// - OpenTruncatedFileOrCreate
// - OpenVerifiedFile   OR   Create
//                           DidCreateOrTruncateForOpen
//                           OpenVerifiedFile
//
// PLATFORM_FILE_OPEN_TRUNCATED:
// - Truncate
// - DidCreateOrTruncateForOpen
// - OpenVerifiedFile
//
void MemoryFileUtil::Open(
    const base::FilePath& file_path,
    int flags,
    const OpenCallback& callback) {
  int create_flag = flags & (base::PLATFORM_FILE_OPEN |
                             base::PLATFORM_FILE_CREATE |
                             base::PLATFORM_FILE_OPEN_ALWAYS |
                             base::PLATFORM_FILE_CREATE_ALWAYS |
                             base::PLATFORM_FILE_OPEN_TRUNCATED);
  switch (create_flag) {
    case base::PLATFORM_FILE_OPEN:
    case base::PLATFORM_FILE_OPEN_ALWAYS:
      GetFileInfo(
          file_path,
          base::Bind(&MemoryFileUtil::DidGetFileInfoForOpen,
                     base::Unretained(this), file_path, flags, callback));

      break;

    case base::PLATFORM_FILE_CREATE:
      Create(
          file_path,
          base::Bind(&MemoryFileUtil::DidCreateOrTruncateForOpen,
                     base::Unretained(this), file_path, flags, 0, callback));
      break;

    case base::PLATFORM_FILE_CREATE_ALWAYS:
      Truncate(
          file_path,
          0,
          base::Bind(&MemoryFileUtil::OpenTruncatedFileOrCreate,
                     base::Unretained(this), file_path, flags, callback));
      break;

    case base::PLATFORM_FILE_OPEN_TRUNCATED:
      Truncate(
          file_path,
          0,
          base::Bind(&MemoryFileUtil::DidCreateOrTruncateForOpen,
                     base::Unretained(this), file_path, flags, 0, callback));
      break;

    default:
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(callback,
                     base::PLATFORM_FILE_ERROR_INVALID_OPERATION,
                     static_cast<AsyncFileStream*>(NULL)));
  }
}

void MemoryFileUtil::GetFileInfo(
    const base::FilePath& file_path,
    const GetFileInfoCallback& callback) {
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&MemoryFileUtil::DoGetFileInfo, base::Unretained(this),
                 file_path.StripTrailingSeparators(), callback));
}

void MemoryFileUtil::Create(
    const base::FilePath& file_path,
    const StatusCallback& callback) {
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&MemoryFileUtil::DoCreate, base::Unretained(this),
                 file_path.StripTrailingSeparators(), false, callback));
}

void MemoryFileUtil::Truncate(
    const base::FilePath& file_path,
    int64 length,
    const StatusCallback& callback) {
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&MemoryFileUtil::DoTruncate, base::Unretained(this),
                 file_path.StripTrailingSeparators(), length, callback));
}

void MemoryFileUtil::Touch(
    const base::FilePath& file_path,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    const StatusCallback& callback) {
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&MemoryFileUtil::DoTouch, base::Unretained(this),
                 file_path.StripTrailingSeparators(),
                 last_modified_time, callback));
}

void MemoryFileUtil::Remove(
    const base::FilePath& file_path,
    bool recursive,
    const StatusCallback& callback) {
  if (recursive) {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&MemoryFileUtil::DoRemoveRecursive,
                   base::Unretained(this), file_path.StripTrailingSeparators(),
                   callback));
  } else {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&MemoryFileUtil::DoRemoveSingleFile,
                   base::Unretained(this), file_path.StripTrailingSeparators(),
                   callback));
  }
}

void MemoryFileUtil::CreateDirectory(
    const base::FilePath& dir_path,
    const StatusCallback& callback) {
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&MemoryFileUtil::DoCreate,
                 base::Unretained(this), dir_path.StripTrailingSeparators(),
                 true, callback));
}

void MemoryFileUtil::ReadDirectory(
    const base::FilePath& dir_path,
    const ReadDirectoryCallback& callback) {
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&MemoryFileUtil::DoReadDirectory,
                 base::Unretained(this), dir_path.StripTrailingSeparators(),
                 base::FilePath(), callback));
}

void MemoryFileUtil::DoGetFileInfo(const base::FilePath& file_path,
                                   const GetFileInfoCallback& callback) {
  base::PlatformFileInfo file_info;

  FileIterator file_it = files_.find(file_path);

  if (file_it == files_.end()) {
    callback.Run(base::PLATFORM_FILE_ERROR_NOT_FOUND, file_info);
    return;
  }
  const FileEntry& file_entry = file_it->second;

  file_info.size = file_entry.contents.size();
  file_info.is_directory = file_entry.is_directory;
  file_info.is_symbolic_link = false;

  // In this file system implementation we store only one datetime. Many
  // popular file systems do the same.
  file_info.last_modified = file_entry.last_modified;
  file_info.last_accessed = file_entry.last_modified;
  file_info.creation_time = file_entry.last_modified;

  callback.Run(base::PLATFORM_FILE_OK, file_info);
}

void MemoryFileUtil::DoCreate(
    const base::FilePath& file_path,
    bool is_directory,
    const StatusCallback& callback) {
  if (FileExists(file_path)) {
    callback.Run(base::PLATFORM_FILE_ERROR_EXISTS);
    return;
  }

  if (!IsDirectory(file_path.DirName())) {
    callback.Run(base::PLATFORM_FILE_ERROR_FAILED);
    return;
  }

  FileEntry file;
  file.is_directory = is_directory;
  file.last_modified = base::Time::Now();

  files_[file_path] = file;
  callback.Run(base::PLATFORM_FILE_OK);
}

void MemoryFileUtil::DoTruncate(
    const base::FilePath& file_path,
    int64 length,
    const StatusCallback& callback) {
  FileIterator file_it = files_.find(file_path);
  if (file_it == files_.end()) {
    callback.Run(base::PLATFORM_FILE_ERROR_NOT_FOUND);
    return;
  }

  FileEntry& file = file_it->second;

  // Fill the extended part with 0 if |length| is larger than the original
  // contents size.
  file.contents.resize(length, 0);
  callback.Run(base::PLATFORM_FILE_OK);
}

void MemoryFileUtil::DoTouch(
    const base::FilePath& file_path,
    const base::Time& last_modified_time,
    const StatusCallback& callback) {
  FileIterator file_it = files_.find(file_path);
  if (file_it == files_.end()) {
    callback.Run(base::PLATFORM_FILE_ERROR_NOT_FOUND);
    return;
  }

  FileEntry& file = file_it->second;

  file.last_modified = last_modified_time;
  callback.Run(base::PLATFORM_FILE_OK);
}

void MemoryFileUtil::DoRemoveSingleFile(
    const base::FilePath& file_path,
    const StatusCallback& callback) {
  FileIterator file_it = files_.find(file_path);
  if (file_it == files_.end()) {
    callback.Run(base::PLATFORM_FILE_ERROR_NOT_FOUND);
    return;
  }

  FileEntry& file = file_it->second;
  if (file.is_directory) {
    // Check if the directory is empty. This can be done by checking if
    // the next file is present under the directory. Note that |files_| is
    // a map hence the file names are sorted by names.
    FileIterator tmp_it = file_it;
    ++tmp_it;
    if (tmp_it != files_.end() && file_path.IsParent(tmp_it->first)) {
      callback.Run(base::PLATFORM_FILE_ERROR_NOT_A_FILE);
      return;
    }
  }

  files_.erase(file_it);
  callback.Run(base::PLATFORM_FILE_OK);
}

void MemoryFileUtil::DoRemoveRecursive(
    const base::FilePath& file_path,
    const StatusCallback& callback) {
  FileIterator file_it = files_.find(file_path);
  if (file_it == files_.end()) {
    callback.Run(base::PLATFORM_FILE_ERROR_NOT_FOUND);
    return;
  }

  FileEntry& file = file_it->second;
  if (!file.is_directory) {
    files_.erase(file_it);
    callback.Run(base::PLATFORM_FILE_OK);
    return;
  }

  // Remove the directory itself.
  files_.erase(file_it++);
  // Remove files under the directory.
  while (file_it != files_.end()) {
    if (file_path.IsParent(file_it->first)) {
      files_.erase(file_it++);
    } else {
      break;
    }
  }
  callback.Run(base::PLATFORM_FILE_OK);
}

void MemoryFileUtil::DoReadDirectory(
    const base::FilePath& dir_path,
    const base::FilePath& in_from,
    const ReadDirectoryCallback& callback) {
  base::FilePath from = in_from;
  read_directory_buffer_.clear();

  if (!FileExists(dir_path)) {
    callback.Run(base::PLATFORM_FILE_ERROR_NOT_FOUND,
                 read_directory_buffer_, true);
    return;
  }

  if (!IsDirectory(dir_path)) {
    callback.Run(base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY,
                 read_directory_buffer_, true);
    return;
  }

  if (from.empty())
    from = dir_path;

  bool completed = true;

  // Here we iterate over all paths in files_ starting with the prefix |from|.
  // It is not very efficient in case of a deep tree with many files in
  // subdirectories. If ever we'll need efficiency from this implementation of
  // FS, this should be changed.
  for (ConstFileIterator it = files_.lower_bound(from);
       it != files_.end();
       ++it) {
    if (dir_path == it->first)  // Skip the directory itself.
      continue;
    if (!dir_path.IsParent(it->first))  // Not in the directory.
      break;
    if (it->first.DirName() != dir_path)  // a file in subdirectory
      continue;

    if (read_directory_buffer_.size() == read_directory_buffer_size_) {
      from = it->first;
      completed = false;
      break;
    }

    const FileEntry& file = it->second;
    DirectoryEntry entry;
    entry.name = it->first.BaseName().value();
    entry.is_directory = file.is_directory;
    entry.size = file.contents.size();
    entry.last_modified_time = file.last_modified;

    read_directory_buffer_.push_back(entry);
  }

  callback.Run(base::PLATFORM_FILE_OK, read_directory_buffer_, completed);

  if (!completed) {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&MemoryFileUtil::DoReadDirectory,
                   base::Unretained(this), dir_path,
                   from, callback));
  }
}

void MemoryFileUtil::OpenVerifiedFile(
    const base::FilePath& file_path,
    int flags,
    const OpenCallback& callback) {
  FileIterator file_it = files_.find(file_path);
  // The existence of the file is guranteed here.
  DCHECK(file_it != files_.end());

  FileEntry* file_entry = &file_it->second;
  callback.Run(base::PLATFORM_FILE_OK,
               new MemoryFileUtilAsyncFileStream(file_entry, flags));
}

void MemoryFileUtil::DidGetFileInfoForOpen(
    const base::FilePath& file_path,
    int flags,
    const OpenCallback& callback,
    PlatformFileError get_info_result,
    const base::PlatformFileInfo& file_info) {
  if (get_info_result == base::PLATFORM_FILE_OK && file_info.is_directory) {
    callback.Run(base::PLATFORM_FILE_ERROR_NOT_A_FILE, NULL);
    return;
  }

  if (get_info_result == base::PLATFORM_FILE_OK) {
    OpenVerifiedFile(file_path, flags, callback);
    return;
  }

  if (get_info_result == base::PLATFORM_FILE_ERROR_NOT_FOUND &&
      flags & base::PLATFORM_FILE_CREATE_ALWAYS) {
    Create(file_path,
           base::Bind(&MemoryFileUtil::DidCreateOrTruncateForOpen,
                      base::Unretained(this), file_path, flags, 0, callback));
    return;
  }

  callback.Run(get_info_result, NULL);
}

void MemoryFileUtil::OpenTruncatedFileOrCreate(
    const base::FilePath& file_path,
    int flags,
    const OpenCallback& callback,
    PlatformFileError result) {
  if (result == base::PLATFORM_FILE_OK) {
    OpenVerifiedFile(file_path, flags, callback);
    return;
  }

  if (result == base::PLATFORM_FILE_ERROR_NOT_FOUND) {
    Create(
        file_path,
        base::Bind(&MemoryFileUtil::DidCreateOrTruncateForOpen,
                   base::Unretained(this), file_path, flags, 0, callback));
    return;
  }

  callback.Run(result, NULL);
}

void MemoryFileUtil::DidCreateOrTruncateForOpen(
    const base::FilePath& file_path,
    int flags,
    int64 size,
    const OpenCallback& callback,
    PlatformFileError result) {
  if (result != base::PLATFORM_FILE_OK) {
    callback.Run(result, NULL);
    return;
  }

  OpenVerifiedFile(file_path, flags, callback);
}

}  // namespace fileapi
