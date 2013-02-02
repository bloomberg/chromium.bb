// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_CHROMEOS_FILEAPI_FILE_SYSTEM_FILE_UTIL_MEMORY_H_
#define WEBKIT_CHROMEOS_FILEAPI_FILE_SYSTEM_FILE_UTIL_MEMORY_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/time.h"
#include "webkit/chromeos/fileapi/file_util_async.h"

namespace fileapi {

// The in-memory file system. Primarily for the purpose of testing
// FileUtilAsync.
class MemoryFileUtil : public FileUtilAsync {
 public:
  struct FileEntry {
    FileEntry();
    ~FileEntry();

    bool is_directory;
    std::string contents;
    base::Time last_modified;
  };

  MemoryFileUtil(const base::FilePath& root_path);
  virtual ~MemoryFileUtil();

  // FileUtilAsync overrides.
  virtual void Open(const base::FilePath& file_path,
                    int file_flags,  // PlatformFileFlags
                    const OpenCallback& callback) OVERRIDE;
  virtual void GetFileInfo(const base::FilePath& file_path,
                           const GetFileInfoCallback& callback) OVERRIDE;
  virtual void Create(const base::FilePath& file_path,
                      const StatusCallback& callback) OVERRIDE;
  virtual void Truncate(const base::FilePath& file_path,
                        int64 length,
                        const StatusCallback& callback) OVERRIDE;
  // This FS ignores last_access_time.
  virtual void Touch(const base::FilePath& file_path,
                     const base::Time& last_access_time,
                     const base::Time& last_modified_time,
                     const StatusCallback& callback) OVERRIDE;
  virtual void Remove(const base::FilePath& file_path,
                      bool recursive,
                      const StatusCallback& callback) OVERRIDE;
  virtual void CreateDirectory(const base::FilePath& dir_path,
                               const StatusCallback& callback) OVERRIDE;
  virtual void ReadDirectory(const base::FilePath& dir_path,
                             const ReadDirectoryCallback& callback) OVERRIDE;

 private:
  friend class MemoryFileUtilTest;

  typedef std::map<base::FilePath, FileEntry>::iterator FileIterator;
  typedef std::map<base::FilePath, FileEntry>::const_iterator ConstFileIterator;

  // Returns true if the given |file_path| is present in the file system.
  bool FileExists(const base::FilePath& file_path) const {
    return files_.find(file_path) != files_.end();
  }

  // Returns true if the given |file_path| is present and a directory.
  bool IsDirectory(const base::FilePath& file_path) const {
    ConstFileIterator it = files_.find(file_path);
    return it != files_.end() && it->second.is_directory;
  }

  // Callback function used to implement GetFileInfo().
  void DoGetFileInfo(const base::FilePath& file_path,
                     const GetFileInfoCallback& callback);

  // Callback function used to implement Create().
  void DoCreate(const base::FilePath& file_path,
                bool is_directory,
                const StatusCallback& callback);

  // Callback function used to implement Truncate().
  void DoTruncate(const base::FilePath& file_path,
                  int64 length,
                  const StatusCallback& callback);

  // Callback function used to implement Touch().
  void DoTouch(const base::FilePath& file_path,
               const base::Time& last_modified_time,
               const StatusCallback& callback);

  // Callback function used to implement Remove().
  void DoRemoveSingleFile(const base::FilePath& file_path,
                          const StatusCallback& callback);

  // Callback function used to implement Remove().
  void DoRemoveRecursive(const base::FilePath& file_path,
                         const StatusCallback& callback);

  // Will start enumerating with file path |from|. If |from| path is
  // empty, will start from the beginning.
  void DoReadDirectory(const base::FilePath& dir_path,
                       const base::FilePath& from,
                       const ReadDirectoryCallback& callback);

  // Opens a file of the given |file_path| with |flags|. A file is
  // guaranteed to be present at |file_path|.
  void OpenVerifiedFile(const base::FilePath& file_path,
                        int flags,
                        const OpenCallback& callback);

  // Callback function used to implement Open().
  void DidGetFileInfoForOpen(const base::FilePath& file_path,
                             int flags,
                             const OpenCallback& callback,
                             PlatformFileError get_info_result,
                             const base::PlatformFileInfo& file_info);

  // Callback function used to implement Open().
  void OpenTruncatedFileOrCreate(const base::FilePath& file_path,
                                 int flags,
                                 const OpenCallback& callback,
                                 PlatformFileError result);

  // Callback function used to implement Open().
  void DidCreateOrTruncateForOpen(const base::FilePath& file_path,
                                  int flags,
                                  int64 size,
                                  const OpenCallback& callback,
                                  PlatformFileError result);

  // Sets the read directory size buffer.
  // Mainly for the purpose of testing.
  void set_read_directory_buffer_size(size_t size) {
    read_directory_buffer_size_ = size;
  }

  // The files in the file system.
  std::map<base::FilePath, FileEntry> files_;
  size_t read_directory_buffer_size_;
  std::vector<DirectoryEntry> read_directory_buffer_;

  DISALLOW_COPY_AND_ASSIGN(MemoryFileUtil);
};

}  // namespace fileapi

#endif  // WEBKIT_CHROMEOS_FILEAPI_FILE_SYSTEM_FILE_UTIL_ASYNC_H_
