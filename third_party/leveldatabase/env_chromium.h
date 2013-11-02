// Copyright (c) 2013 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef THIRD_PARTY_LEVELDATABASE_ENV_CHROMIUM_H_
#define THIRD_PARTY_LEVELDATABASE_ENV_CHROMIUM_H_

#include <deque>
#include <map>
#include <set>

#include "base/metrics/histogram.h"
#include "base/platform_file.h"
#include "base/synchronization/condition_variable.h"
#include "leveldb/env.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"
#include "port/port_chromium.h"
#include "util/mutexlock.h"

namespace leveldb_env {

enum MethodID {
  kSequentialFileRead,
  kSequentialFileSkip,
  kRandomAccessFileRead,
  kWritableFileAppend,
  kWritableFileClose,
  kWritableFileFlush,
  kWritableFileSync,
  kNewSequentialFile,
  kNewRandomAccessFile,
  kNewWritableFile,
  kDeleteFile,
  kCreateDir,
  kDeleteDir,
  kGetFileSize,
  kRenameFile,
  kLockFile,
  kUnlockFile,
  kGetTestDirectory,
  kNewLogger,
  kSyncParent,
  kGetChildren,
  kNumEntries
};

const char* MethodIDToString(MethodID method);

leveldb::Status MakeIOError(leveldb::Slice filename,
                            const char* message,
                            MethodID method,
                            int saved_errno);
leveldb::Status MakeIOError(leveldb::Slice filename,
                            const char* message,
                            MethodID method,
                            base::PlatformFileError error);
leveldb::Status MakeIOError(leveldb::Slice filename,
                            const char* message,
                            MethodID method);

enum ErrorParsingResult {
  METHOD_ONLY,
  METHOD_AND_PFE,
  METHOD_AND_ERRNO,
  NONE,
};

ErrorParsingResult ParseMethodAndError(const char* string,
                                       MethodID* method,
                                       int* error);
int GetCorruptionCode(const leveldb::Status& status);
int GetNumCorruptionCodes();
std::string GetCorruptionMessage(const leveldb::Status& status);
bool IndicatesDiskFull(const leveldb::Status& status);
bool IsIOError(const leveldb::Status& status);
bool IsCorruption(const leveldb::Status& status);
std::string FilePathToString(const base::FilePath& file_path);

class UMALogger {
 public:
  virtual void RecordErrorAt(MethodID method) const = 0;
  virtual void RecordOSError(MethodID method, int saved_errno) const = 0;
  virtual void RecordOSError(MethodID method,
                             base::PlatformFileError error) const = 0;
  virtual void RecordBackupResult(bool success) const = 0;
};

class RetrierProvider {
 public:
  virtual int MaxRetryTimeMillis() const = 0;
  virtual base::HistogramBase* GetRetryTimeHistogram(MethodID method) const = 0;
  virtual base::HistogramBase* GetRecoveredFromErrorHistogram(
      MethodID method) const = 0;
};

class WriteTracker {
 public:
  virtual void DidCreateNewFile(const std::string& fname) = 0;
  virtual bool DoesDirNeedSync(const std::string& fname) = 0;
  virtual void DidSyncDir(const std::string& fname) = 0;
};

class ChromiumWritableFile : public leveldb::WritableFile {
 public:
  ChromiumWritableFile(const std::string& fname,
                       FILE* f,
                       const UMALogger* uma_logger,
                       WriteTracker* tracker,
                       bool make_backup);
  virtual ~ChromiumWritableFile();
  virtual leveldb::Status Append(const leveldb::Slice& data);
  virtual leveldb::Status Close();
  virtual leveldb::Status Flush();
  virtual leveldb::Status Sync();

 private:
  enum Type {
    kManifest,
    kTable,
    kOther
  };
  leveldb::Status SyncParent();

  std::string filename_;
  FILE* file_;
  const UMALogger* uma_logger_;
  WriteTracker* tracker_;
  Type file_type_;
  std::string parent_dir_;
  bool make_backup_;
};

class ChromiumEnv : public leveldb::Env,
                    public UMALogger,
                    public RetrierProvider,
                    public WriteTracker {
 public:
  ChromiumEnv();
  virtual ~ChromiumEnv();

  virtual leveldb::Status NewSequentialFile(const std::string& fname,
                                            leveldb::SequentialFile** result);
  virtual leveldb::Status NewRandomAccessFile(
      const std::string& fname,
      leveldb::RandomAccessFile** result);
  virtual leveldb::Status NewWritableFile(const std::string& fname,
                                          leveldb::WritableFile** result);
  virtual bool FileExists(const std::string& fname);
  virtual leveldb::Status GetChildren(const std::string& dir,
                                      std::vector<std::string>* result);
  virtual leveldb::Status DeleteFile(const std::string& fname);
  virtual leveldb::Status CreateDir(const std::string& name);
  virtual leveldb::Status DeleteDir(const std::string& name);
  virtual leveldb::Status GetFileSize(const std::string& fname, uint64_t* size);
  virtual leveldb::Status RenameFile(const std::string& src,
                                     const std::string& dst);
  virtual leveldb::Status LockFile(const std::string& fname,
                                   leveldb::FileLock** lock);
  virtual leveldb::Status UnlockFile(leveldb::FileLock* lock);
  virtual void Schedule(void (*function)(void*), void* arg);
  virtual void StartThread(void (*function)(void* arg), void* arg);
  virtual leveldb::Status GetTestDirectory(std::string* path);
  virtual leveldb::Status NewLogger(const std::string& fname,
                                    leveldb::Logger** result);
  virtual uint64_t NowMicros();
  virtual void SleepForMicroseconds(int micros);

 protected:
  virtual void DidCreateNewFile(const std::string& fname);
  virtual bool DoesDirNeedSync(const std::string& fname);
  virtual void DidSyncDir(const std::string& fname);

  std::string name_;
  bool make_backup_;

 private:
  // File locks may not be exclusive within a process (e.g. on POSIX). Track
  // locks held by the ChromiumEnv to prevent access within the process.
  class LockTable {
   public:
    bool Insert(const std::string& fname) {
      leveldb::MutexLock l(&mu_);
      return locked_files_.insert(fname).second;
    }
    bool Remove(const std::string& fname) {
      leveldb::MutexLock l(&mu_);
      return locked_files_.erase(fname) == 1;
    }
   private:
    leveldb::port::Mutex mu_;
    std::set<std::string> locked_files_;
  };

  std::map<std::string, bool> needs_sync_map_;
  base::Lock map_lock_;

  const int kMaxRetryTimeMillis;
  // BGThread() is the body of the background thread
  void BGThread();
  static void BGThreadWrapper(void* arg) {
    reinterpret_cast<ChromiumEnv*>(arg)->BGThread();
  }

  virtual void RecordErrorAt(MethodID method) const;
  virtual void RecordOSError(MethodID method, int saved_errno) const;
  virtual void RecordOSError(MethodID method,
                             base::PlatformFileError error) const;
  virtual void RecordBackupResult(bool result) const;
  void RestoreIfNecessary(const std::string& dir,
                          std::vector<std::string>* children);
  base::FilePath RestoreFromBackup(const base::FilePath& base_name);
  void RecordOpenFilesLimit(const std::string& type);
  void RecordLockFileAncestors(int num_missing_ancestors) const;
  base::HistogramBase* GetOSErrorHistogram(MethodID method, int limit) const;
  base::HistogramBase* GetMethodIOErrorHistogram() const;
  base::HistogramBase* GetMaxFDHistogram(const std::string& type) const;
  base::HistogramBase* GetLockFileAncestorHistogram() const;

  // RetrierProvider implementation.
  virtual int MaxRetryTimeMillis() const { return kMaxRetryTimeMillis; }
  virtual base::HistogramBase* GetRetryTimeHistogram(MethodID method) const;
  virtual base::HistogramBase* GetRecoveredFromErrorHistogram(
      MethodID method) const;

  base::FilePath test_directory_;

  ::base::Lock mu_;
  ::base::ConditionVariable bgsignal_;
  bool started_bgthread_;

  // Entry per Schedule() call
  struct BGItem {
    void* arg;
    void (*function)(void*);
  };
  typedef std::deque<BGItem> BGQueue;
  BGQueue queue_;
  LockTable locks_;
};

}  // namespace leveldb_env

#endif
