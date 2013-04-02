// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include <errno.h>
#include <stdio.h>

#include <deque>

#include "base/at_exit.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/platform_file.h"
#include "base/posix/eintr_wrapper.h"
#include "base/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/sys_info.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "chromium_logger.h"
#include "leveldb/env.h"
#include "leveldb/slice.h"
#include "port/port.h"
#include "util/logging.h"

#if defined(OS_WIN)
#include <io.h>
#include "base/win/win_util.h"
#endif

#if !defined(OS_WIN)
#include <fcntl.h>
#endif

namespace {

#if defined(OS_MACOSX) || defined(OS_WIN) || defined(OS_ANDROID) || \
    defined(OS_OPENBSD)
// The following are glibc-specific

size_t fread_unlocked(void *ptr, size_t size, size_t n, FILE *file) {
  return fread(ptr, size, n, file);
}

size_t fwrite_unlocked(const void *ptr, size_t size, size_t n, FILE *file) {
  return fwrite(ptr, size, n, file);
}

int fflush_unlocked(FILE *file) {
  return fflush(file);
}

#if !defined(OS_ANDROID)
int fdatasync(int fildes) {
#if defined(OS_WIN)
  return _commit(fildes);
#else
  return HANDLE_EINTR(fsync(fildes));
#endif
}
#endif

#endif

// Wide-char safe fopen wrapper.
FILE* fopen_internal(const char* fname, const char* mode) {
#if defined(OS_WIN)
  return _wfopen(UTF8ToUTF16(fname).c_str(), ASCIIToUTF16(mode).c_str());
#else
  return fopen(fname, mode);
#endif
}

base::FilePath CreateFilePath(const std::string& file_path) {
#if defined(OS_WIN)
  return base::FilePath(UTF8ToUTF16(file_path));
#else
  return base::FilePath(file_path);
#endif
}

std::string FilePathToString(const base::FilePath& file_path) {
#if defined(OS_WIN)
  return UTF16ToUTF8(file_path.value());
#else
  return file_path.value();
#endif
}

bool sync_parent(const std::string& fname) {
#if !defined(OS_WIN)
  base::FilePath parent_dir = CreateFilePath(fname).DirName();
  int parent_fd =
      HANDLE_EINTR(open(FilePathToString(parent_dir).c_str(), O_RDONLY));
  if (parent_fd < 0)
    return false;
  HANDLE_EINTR(fsync(parent_fd));
  HANDLE_EINTR(close(parent_fd));
#endif
  return true;
}

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
  kRenamefile,
  kLockFile,
  kUnlockFile,
  kGetTestDirectory,
  kNewLogger,
  kNumEntries
};

const char* MethodIDToString(MethodID method) {
  switch (method) {
    case kSequentialFileRead:
      return "SequentialFileRead";
    case kSequentialFileSkip:
      return "SequentialFileSkip";
    case kRandomAccessFileRead:
      return "RandomAccessFileRead";
    case kWritableFileAppend:
      return "WritableFileAppend";
    case kWritableFileClose:
      return "WritableFileClose";
    case kWritableFileFlush:
      return "WritableFileFlush";
    case kWritableFileSync:
      return "WritableFileSync";
    case kNewSequentialFile:
      return "NewSequentialFile";
    case kNewRandomAccessFile:
      return "NewRandomAccessFile";
    case kNewWritableFile:
      return "NewWritableFile";
    case kDeleteFile:
      return "DeleteFile";
    case kCreateDir:
      return "CreateDir";
    case kDeleteDir:
      return "DeleteDir";
    case kGetFileSize:
      return "GetFileSize";
    case kRenamefile:
      return "Renamefile";
    case kLockFile:
      return "LockFile";
    case kUnlockFile:
      return "UnlockFile";
    case kGetTestDirectory:
      return "GetTestDirectory";
    case kNewLogger:
      return "NewLogger";
    case kNumEntries:
      NOTREACHED();
      return "kNumEntries";
  }
  NOTREACHED();
  return "Unknown";
}

class UMALogger {
 public:
  virtual void RecordErrorAt(MethodID method) const = 0;
  virtual void RecordSpecificError(MethodID method, int saved_errno) const = 0;
  virtual void RecordSpecificError(MethodID method,
                                   base::PlatformFileError error) const = 0;
};

}  // namespace

namespace leveldb {

namespace {

class Thread;

static const base::FilePath::CharType kLevelDBTestDirectoryPrefix[]
    = FILE_PATH_LITERAL("leveldb-test-");

const char* PlatformFileErrorString(const ::base::PlatformFileError& error) {
  switch (error) {
    case ::base::PLATFORM_FILE_ERROR_FAILED:
      return "Opening file failed.";
    case ::base::PLATFORM_FILE_ERROR_IN_USE:
      return "File currently in use.";
    case ::base::PLATFORM_FILE_ERROR_EXISTS:
      return "File already exists.";
    case ::base::PLATFORM_FILE_ERROR_NOT_FOUND:
      return "File not found.";
    case ::base::PLATFORM_FILE_ERROR_ACCESS_DENIED:
      return "Access denied.";
    case ::base::PLATFORM_FILE_ERROR_TOO_MANY_OPENED:
      return "Too many files open.";
    case ::base::PLATFORM_FILE_ERROR_NO_MEMORY:
      return "Out of memory.";
    case ::base::PLATFORM_FILE_ERROR_NO_SPACE:
      return "No space left on drive.";
    case ::base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY:
      return "Not a directory.";
    case ::base::PLATFORM_FILE_ERROR_INVALID_OPERATION:
      return "Invalid operation.";
    case ::base::PLATFORM_FILE_ERROR_SECURITY:
      return "Security error.";
    case ::base::PLATFORM_FILE_ERROR_ABORT:
      return "File operation aborted.";
    case ::base::PLATFORM_FILE_ERROR_NOT_A_FILE:
      return "The supplied path was not a file.";
    case ::base::PLATFORM_FILE_ERROR_NOT_EMPTY:
      return "The file was not empty.";
    case ::base::PLATFORM_FILE_ERROR_INVALID_URL:
      return "Invalid URL.";
    case ::base::PLATFORM_FILE_OK:
      return "OK.";
    case ::base::PLATFORM_FILE_ERROR_MAX:
      NOTREACHED();
  }
  NOTIMPLEMENTED();
  return "Unknown error.";
}

class ChromiumSequentialFile: public SequentialFile {
 private:
  std::string filename_;
  FILE* file_;
  const UMALogger* uma_logger_;

 public:
  ChromiumSequentialFile(const std::string& fname, FILE* f,
                         const UMALogger* uma_logger)
      : filename_(fname), file_(f), uma_logger_(uma_logger) { }
  virtual ~ChromiumSequentialFile() { fclose(file_); }

  virtual Status Read(size_t n, Slice* result, char* scratch) {
    Status s;
    size_t r = fread_unlocked(scratch, 1, n, file_);
    *result = Slice(scratch, r);
    if (r < n) {
      if (feof(file_)) {
        // We leave status as ok if we hit the end of the file
      } else {
        // A partial read with an error: return a non-ok status
        s = Status::IOError(filename_, strerror(errno));
        uma_logger_->RecordErrorAt(kSequentialFileRead);
      }
    }
    return s;
  }

  virtual Status Skip(uint64_t n) {
    if (fseek(file_, n, SEEK_CUR)) {
      uma_logger_->RecordErrorAt(kSequentialFileSkip);
      return Status::IOError(filename_, strerror(errno));
    }
    return Status::OK();
  }
};

class ChromiumRandomAccessFile: public RandomAccessFile {
 private:
  std::string filename_;
  ::base::PlatformFile file_;
  const UMALogger* uma_logger_;

 public:
  ChromiumRandomAccessFile(const std::string& fname, ::base::PlatformFile file,
                           const UMALogger* uma_logger)
      : filename_(fname), file_(file), uma_logger_(uma_logger) { }
  virtual ~ChromiumRandomAccessFile() { ::base::ClosePlatformFile(file_); }

  virtual Status Read(uint64_t offset, size_t n, Slice* result,
                      char* scratch) const {
    Status s;
    int r = ::base::ReadPlatformFile(file_, offset, scratch, n);
    *result = Slice(scratch, (r < 0) ? 0 : r);
    if (r < 0) {
      // An error: return a non-ok status
      s = Status::IOError(filename_, "Could not perform read");
      uma_logger_->RecordErrorAt(kRandomAccessFileRead);
    }
    return s;
  }
};

class ChromiumWritableFile : public WritableFile {
 private:
  std::string filename_;
  FILE* file_;
  const UMALogger* uma_logger_;

 public:
  ChromiumWritableFile(const std::string& fname, FILE* f,
                       const UMALogger* uma_logger)
      : filename_(fname), file_(f), uma_logger_(uma_logger) { }

  ~ChromiumWritableFile() {
    if (file_ != NULL) {
      // Ignoring any potential errors
      fclose(file_);
    }
  }

  virtual Status Append(const Slice& data) {
    size_t r = fwrite_unlocked(data.data(), 1, data.size(), file_);
    Status result;
    if (r != data.size()) {
      uma_logger_->RecordSpecificError(kWritableFileAppend, errno);
      result = Status::IOError(filename_, strerror(errno));
    }
    return result;
  }

  virtual Status Close() {
    Status result;
    if (fclose(file_) != 0) {
      result = Status::IOError(filename_, strerror(errno));
      uma_logger_->RecordErrorAt(kWritableFileClose);
    }
    file_ = NULL;
    return result;
  }

  virtual Status Flush() {
    Status result;
    if (HANDLE_EINTR(fflush_unlocked(file_))) {
      result = Status::IOError(filename_, strerror(errno));
      uma_logger_->RecordErrorAt(kWritableFileFlush);
    }
    return result;
  }

  virtual Status Sync() {
    Status result;
    int error = 0;

    if (HANDLE_EINTR(fflush_unlocked(file_)))
      error = errno;
    // Sync even if fflush gave an error; perhaps the data actually got out,
    // even though something went wrong.
    if (fdatasync(fileno(file_)) && !error)
      error = errno;
    // Report the first error we found.
    if (error) {
      result = Status::IOError(filename_, strerror(error));
      uma_logger_->RecordErrorAt(kWritableFileSync);
    }
    return result;
  }
};

class ChromiumFileLock : public FileLock {
 public:
  ::base::PlatformFile file_;
};

class ChromiumEnv : public Env, public UMALogger {
 public:
  ChromiumEnv();
  virtual ~ChromiumEnv() {
    NOTREACHED();
  }

  virtual Status NewSequentialFile(const std::string& fname,
                                   SequentialFile** result) {
    FILE* f = fopen_internal(fname.c_str(), "rb");
    if (f == NULL) {
      *result = NULL;
      RecordErrorAt(kNewSequentialFile);
      return Status::IOError(fname, strerror(errno));
    } else {
      *result = new ChromiumSequentialFile(fname, f, this);
      return Status::OK();
    }
  }

  virtual Status NewRandomAccessFile(const std::string& fname,
                                     RandomAccessFile** result) {
    int flags = ::base::PLATFORM_FILE_READ | ::base::PLATFORM_FILE_OPEN;
    bool created;
    ::base::PlatformFileError error_code;
    ::base::PlatformFile file = ::base::CreatePlatformFile(
        CreateFilePath(fname), flags, &created, &error_code);
    if (error_code != ::base::PLATFORM_FILE_OK) {
      *result = NULL;
      RecordSpecificError(kNewRandomAccessFile, error_code);
      return Status::IOError(fname, PlatformFileErrorString(error_code));
    }
    *result = new ChromiumRandomAccessFile(fname, file, this);
    return Status::OK();
  }

  virtual Status NewWritableFile(const std::string& fname,
                                 WritableFile** result) {
    *result = NULL;
    FILE* f = fopen_internal(fname.c_str(), "wb");
    if (f == NULL) {
      RecordErrorAt(kNewWritableFile);
      return Status::IOError(fname, strerror(errno));
    } else {
      if (!sync_parent(fname)) {
        fclose(f);
        RecordErrorAt(kNewWritableFile);
        return Status::IOError(fname, strerror(errno));
      }
      *result = new ChromiumWritableFile(fname, f, this);
      return Status::OK();
    }
  }

  virtual bool FileExists(const std::string& fname) {
    return ::file_util::PathExists(CreateFilePath(fname));
  }

  virtual Status GetChildren(const std::string& dir,
                             std::vector<std::string>* result) {
    result->clear();
    ::file_util::FileEnumerator iter(
        CreateFilePath(dir), false, ::file_util::FileEnumerator::FILES);
    base::FilePath current = iter.Next();
    while (!current.empty()) {
      result->push_back(FilePathToString(current.BaseName()));
      current = iter.Next();
    }
    // TODO(jorlow): Unfortunately, the FileEnumerator swallows errors, so
    //               we'll always return OK. Maybe manually check for error
    //               conditions like the file not existing?
    return Status::OK();
  }

  virtual Status DeleteFile(const std::string& fname) {
    Status result;
    // TODO(jorlow): Should we assert this is a file?
    if (!::file_util::Delete(CreateFilePath(fname), false)) {
      result = Status::IOError(fname, "Could not delete file.");
      RecordErrorAt(kDeleteFile);
    }
    return result;
  };

  virtual Status CreateDir(const std::string& name) {
    Status result;
    if (!::file_util::CreateDirectory(CreateFilePath(name))) {
      result = Status::IOError(name, "Could not create directory.");
      RecordErrorAt(kCreateDir);
    }
    return result;
  };

  virtual Status DeleteDir(const std::string& name) {
    Status result;
    // TODO(jorlow): Should we assert this is a directory?
    if (!::file_util::Delete(CreateFilePath(name), false)) {
      result = Status::IOError(name, "Could not delete directory.");
      RecordErrorAt(kDeleteDir);
    }
    return result;
  };

  virtual Status GetFileSize(const std::string& fname, uint64_t* size) {
    Status s;
    int64_t signed_size;
    if (!::file_util::GetFileSize(CreateFilePath(fname), &signed_size)) {
      *size = 0;
      s = Status::IOError(fname, "Could not determine file size.");
      RecordErrorAt(kGetFileSize);
    } else {
      *size = static_cast<uint64_t>(signed_size);
    }
    return s;
  }

  class Retrier {
   public:
    Retrier(base::HistogramBase* histogram, const int kMaxRetryMillis) :
        start_(base::TimeTicks::Now()),
        limit_(start_ + base::TimeDelta::FromMilliseconds(kMaxRetryMillis)),
        last_(start_),
        time_to_sleep_(base::TimeDelta::FromMilliseconds(10)),
        success_(true),
        histogram_(histogram) {
    }
    ~Retrier() {
      if (success_)
        histogram_->AddTime(last_ - start_);
    }
    bool ShouldKeepTrying() {
      if (last_ < limit_) {
        base::PlatformThread::Sleep(time_to_sleep_);
        last_ = base::TimeTicks::Now();
        return true;
      }
      success_ = false;
      return false;
    }
   private:
    base::TimeTicks start_;
    base::TimeTicks limit_;
    base::TimeTicks last_;
    base::TimeDelta time_to_sleep_;
    bool success_;
    base::HistogramBase* histogram_;
  };

  virtual Status RenameFile(const std::string& src, const std::string& dst) {
    Status result;
    base::FilePath src_file_path = CreateFilePath(src);
    if (!::file_util::PathExists(src_file_path))
      return result;
    base::FilePath destination = CreateFilePath(dst);

    Retrier r(rename_time_histogram_, kMaxRenameTimeMillis);
    do {
      if (::file_util::ReplaceFile(src_file_path, destination)) {
        sync_parent(dst);
        if (src != dst)
          sync_parent(src);
        return result;
      }
    } while (r.ShouldKeepTrying());

    RecordErrorAt(kRenamefile);
    return Status::IOError(src, "Could not rename file.");
  }

  virtual Status LockFile(const std::string& fname, FileLock** lock) {
    *lock = NULL;
    Status result;
    int flags = ::base::PLATFORM_FILE_OPEN_ALWAYS |
                ::base::PLATFORM_FILE_READ |
                ::base::PLATFORM_FILE_WRITE |
                ::base::PLATFORM_FILE_EXCLUSIVE_READ |
                ::base::PLATFORM_FILE_EXCLUSIVE_WRITE;
    bool created;
    ::base::PlatformFileError error_code;
    ::base::PlatformFile file = ::base::CreatePlatformFile(
        CreateFilePath(fname), flags, &created, &error_code);
    if (error_code != ::base::PLATFORM_FILE_OK) {
      result = Status::IOError(fname, PlatformFileErrorString(error_code));
      RecordErrorAt(kLockFile);
    } else {
      ChromiumFileLock* my_lock = new ChromiumFileLock;
      my_lock->file_ = file;
      *lock = my_lock;
    }
    return result;
  }

  virtual Status UnlockFile(FileLock* lock) {
    ChromiumFileLock* my_lock = reinterpret_cast<ChromiumFileLock*>(lock);
    Status result;
    if (!::base::ClosePlatformFile(my_lock->file_)) {
      result = Status::IOError("Could not close lock file.");
      RecordErrorAt(kUnlockFile);
    }
    delete my_lock;
    return result;
  }

  virtual void Schedule(void (*function)(void*), void* arg);

  virtual void StartThread(void (*function)(void* arg), void* arg);

  virtual std::string UserIdentifier() {
#if defined(OS_WIN)
    std::wstring user_sid;
    bool ret = ::base::win::GetUserSidString(&user_sid);
    DCHECK(ret);
    return UTF16ToUTF8(user_sid);
#else
    char buf[100];
    snprintf(buf, sizeof(buf), "%d", int(geteuid()));
    return buf;
#endif
  }

  virtual Status GetTestDirectory(std::string* path) {
    mu_.Acquire();
    if (test_directory_.empty()) {
      if (!::file_util::CreateNewTempDirectory(kLevelDBTestDirectoryPrefix,
                                               &test_directory_)) {
        mu_.Release();
        RecordErrorAt(kGetTestDirectory);
        return Status::IOError("Could not create temp directory.");
      }
    }
    *path = FilePathToString(test_directory_);
    mu_.Release();
    return Status::OK();
  }

  virtual Status NewLogger(const std::string& fname, Logger** result) {
    FILE* f = fopen_internal(fname.c_str(), "w");
    if (f == NULL) {
      *result = NULL;
      RecordErrorAt(kNewLogger);
      return Status::IOError(fname, strerror(errno));
    } else {
      if (!sync_parent(fname)) {
        fclose(f);
        return Status::IOError(fname, strerror(errno));
      }
      *result = new ChromiumLogger(f);
      return Status::OK();
    }
  }

  virtual uint64_t NowMicros() {
    return ::base::TimeTicks::Now().ToInternalValue();
  }

  virtual void SleepForMicroseconds(int micros) {
    // Round up to the next millisecond.
    ::base::PlatformThread::Sleep(::base::TimeDelta::FromMicroseconds(micros));
  }

  void RecordErrorAt(MethodID method) const {
    io_error_histogram_->Add(method);
  }

  void RecordSpecificError(MethodID method, base::PlatformFileError error)
      const {
    DCHECK(error < 0);
    RecordSpecificError(method, -error);
  }

  void RecordSpecificError(MethodID method, int error) const {
    RecordErrorAt(method);
    if (error_histograms_.find(method) == error_histograms_.end()) {
      NOTREACHED();
      return;
    }
    error_histograms_.find(method)->second->Add(error);
  }

  void RecordTimeToRename(base::TimeDelta t) const {
    rename_time_histogram_->AddTime(t);
  }

 protected:
  void InitHistograms(const std::string& uma_title);
  void MakePlatformFileErrorHistogram(const std::string& prefix_with_dot,
                                      MethodID method);
  void MakeErrnoHistogram(const std::string& prefix_with_dot,
                          MethodID method);
  void MakeErrorHistogram(const std::string& prefix_with_dot,
                          MethodID method,
                          int limit);

 private:
  const int kMaxRenameTimeMillis;
  // BGThread() is the body of the background thread
  void BGThread();
  static void BGThreadWrapper(void* arg) {
    reinterpret_cast<ChromiumEnv*>(arg)->BGThread();
  }

  base::FilePath test_directory_;

  size_t page_size_;
  ::base::Lock mu_;
  ::base::ConditionVariable bgsignal_;
  bool started_bgthread_;

  // Entry per Schedule() call
  struct BGItem { void* arg; void (*function)(void*); };
  typedef std::deque<BGItem> BGQueue;
  BGQueue queue_;

  base::HistogramBase* io_error_histogram_;
  base::HistogramBase* random_access_file_histogram_;
  base::HistogramBase* rename_time_histogram_;
  std::map<MethodID, base::HistogramBase*> error_histograms_;
};

ChromiumEnv::ChromiumEnv()
    : page_size_(::base::SysInfo::VMAllocationGranularity()),
      bgsignal_(&mu_),
      started_bgthread_(false),
      kMaxRenameTimeMillis(1000) {
  InitHistograms("LevelDBEnv");
}

void ChromiumEnv::MakePlatformFileErrorHistogram(
    const std::string& prefix_with_dot, MethodID method) {
  MakeErrorHistogram(prefix_with_dot, method, -base::PLATFORM_FILE_ERROR_MAX);
}

void ChromiumEnv::MakeErrnoHistogram(const std::string& prefix_with_dot,
                                     MethodID method) {
  MakeErrorHistogram(prefix_with_dot, method, ERANGE + 1);
}

void ChromiumEnv::MakeErrorHistogram(const std::string& prefix_with_dot,
                                     MethodID method,
                                     int limit) {
  std::string uma_name(prefix_with_dot);
  uma_name.append(MethodIDToString(method));
  error_histograms_[method] = base::LinearHistogram::FactoryGet(uma_name, 1,
    limit, limit + 1, base::Histogram::kUmaTargetedHistogramFlag);
}

void ChromiumEnv::InitHistograms(const std::string& uma_title) {
  std::string uma_name(uma_title);
  uma_name.append(".IOError");
  io_error_histogram_ = base::LinearHistogram::FactoryGet(uma_name, 1,
      kNumEntries, kNumEntries + 1, base::Histogram::kUmaTargetedHistogramFlag);

  uma_name.append(".");
  MakeErrnoHistogram(uma_name, kWritableFileAppend);
  MakePlatformFileErrorHistogram(uma_name, kNewRandomAccessFile);

  std::string retry_name(uma_title);
  retry_name.append(".TimeToRename");
  const int kBucketSizeMillis = 25;
  // Add 2, 1 for each of the buckets <1 and >max.
  int num_buckets = kMaxRenameTimeMillis / kBucketSizeMillis + 2;
  rename_time_histogram_ = base::LinearHistogram::FactoryTimeGet(
      retry_name, base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromMilliseconds(kMaxRenameTimeMillis + 1),
      num_buckets,
      base::Histogram::kUmaTargetedHistogramFlag);
}

class Thread : public ::base::PlatformThread::Delegate {
 public:
  Thread(void (*function)(void* arg), void* arg)
      : function_(function), arg_(arg) {
    ::base::PlatformThreadHandle handle;
    bool success = ::base::PlatformThread::Create(0, this, &handle);
    DCHECK(success);
  }
  virtual ~Thread() {}
  virtual void ThreadMain() {
    (*function_)(arg_);
    delete this;
  }

 private:
  void (*function_)(void* arg);
  void* arg_;
};

void ChromiumEnv::Schedule(void (*function)(void*), void* arg) {
  mu_.Acquire();

  // Start background thread if necessary
  if (!started_bgthread_) {
    started_bgthread_ = true;
    StartThread(&ChromiumEnv::BGThreadWrapper, this);
  }

  // If the queue is currently empty, the background thread may currently be
  // waiting.
  if (queue_.empty()) {
    bgsignal_.Signal();
  }

  // Add to priority queue
  queue_.push_back(BGItem());
  queue_.back().function = function;
  queue_.back().arg = arg;

  mu_.Release();
}

void ChromiumEnv::BGThread() {
  while (true) {
    // Wait until there is an item that is ready to run
    mu_.Acquire();
    while (queue_.empty()) {
      bgsignal_.Wait();
    }

    void (*function)(void*) = queue_.front().function;
    void* arg = queue_.front().arg;
    queue_.pop_front();

    mu_.Release();
    (*function)(arg);
  }
}

void ChromiumEnv::StartThread(void (*function)(void* arg), void* arg) {
  new Thread(function, arg); // Will self-delete.
}

class IDBEnv : public ChromiumEnv {
 public:
  IDBEnv() : ChromiumEnv() {
    InitHistograms("LevelDBEnv.IDB");
  }
};

::base::LazyInstance<IDBEnv>::Leaky
    idb_env = LAZY_INSTANCE_INITIALIZER;

::base::LazyInstance<ChromiumEnv>::Leaky
    default_env = LAZY_INSTANCE_INITIALIZER;

}

Env* IDBEnv() {
  return idb_env.Pointer();
}

Env* Env::Default() {
  return default_env.Pointer();
}

}
