// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_FILEAPI_SANDBOX_CONTEXT_H_
#define WEBKIT_BROWSER_FILEAPI_SANDBOX_CONTEXT_H_

#include <set>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "webkit/browser/fileapi/file_system_backend.h"
#include "webkit/browser/fileapi/file_system_options.h"
#include "webkit/browser/fileapi/file_system_quota_util.h"
#include "webkit/browser/webkit_storage_browser_export.h"

namespace base {
class SequencedTaskRunner;
}

namespace quota {
class QuotaManagerProxy;
class SpecialStoragePolicy;
}

namespace fileapi {

class AsyncFileUtilAdapter;
class FileSystemURL;
class FileSystemUsageCache;
class ObfuscatedFileUtil;
class SandboxFileSystemBackend;
class SandboxFileSystemTestHelper;
class SandboxQuotaObserver;

// This class keeps and provides a sandbox file system context.
// An instance of this class is created and owned by FileSystemContext.
class WEBKIT_STORAGE_BROWSER_EXPORT SandboxContext {
 public:
  typedef FileSystemBackend::OpenFileSystemCallback OpenFileSystemCallback;

  // The FileSystem directory name.
  static const base::FilePath::CharType kFileSystemDirectory[];

  // Origin enumerator interface.
  // An instance of this interface is assumed to be called on the file thread.
  class OriginEnumerator {
   public:
    virtual ~OriginEnumerator() {}

    // Returns the next origin.  Returns empty if there are no more origins.
    virtual GURL Next() = 0;

    // Returns the current origin's information.
    virtual bool HasFileSystemType(FileSystemType type) const = 0;
  };

  SandboxContext(
      quota::QuotaManagerProxy* quota_manager_proxy,
      base::SequencedTaskRunner* file_task_runner,
      const base::FilePath& profile_path,
      quota::SpecialStoragePolicy* special_storage_policy,
      const FileSystemOptions& file_system_options);

  ~SandboxContext();

  // Performs API-specific validity checks on the given path |url|.
  // Returns true if access to |url| is valid in this filesystem.
  bool IsAccessValid(const FileSystemURL& url) const;

  // Returns true if the given |url|'s scheme is allowed to access
  // filesystem.
  bool IsAllowedScheme(const GURL& url) const;

  // Returns an origin enumerator of sandbox filesystem.
  // This method can only be called on the file thread.
  OriginEnumerator* CreateOriginEnumerator();

  // Gets a base directory path of the sandboxed filesystem that is
  // specified by |origin_url| and |type|.
  // (The path is similar to the origin's root path but doesn't contain
  // the 'unique' part.)
  // Returns an empty path if the given type is invalid.
  // This method can only be called on the file thread.
  base::FilePath GetBaseDirectoryForOriginAndType(
      const GURL& origin_url,
      FileSystemType type,
      bool create);

  // FileSystemBackend helpers.
  void OpenFileSystem(
      const GURL& origin_url,
      FileSystemType type,
      OpenFileSystemMode mode,
      const OpenFileSystemCallback& callback,
      const GURL& root_url);

  // FileSystemQuotaUtil helpers.
  base::PlatformFileError DeleteOriginDataOnFileThread(
      FileSystemContext* context,
      quota::QuotaManagerProxy* proxy,
      const GURL& origin_url,
      FileSystemType type);
  void GetOriginsForTypeOnFileThread(
      FileSystemType type,
      std::set<GURL>* origins);
  void GetOriginsForHostOnFileThread(
      FileSystemType type,
      const std::string& host,
      std::set<GURL>* origins);
  int64 GetOriginUsageOnFileThread(
      FileSystemContext* context,
      const GURL& origin_url,
      FileSystemType type);
  void InvalidateUsageCache(
      const GURL& origin_url,
      FileSystemType type);
  void StickyInvalidateUsageCache(
      const GURL& origin_url,
      FileSystemType type);

  void CollectOpenFileSystemMetrics(base::PlatformFileError error_code);

  base::SequencedTaskRunner* file_task_runner() {
    return file_task_runner_.get();
  }

  AsyncFileUtilAdapter* file_util() { return sandbox_file_util_.get(); }
  FileSystemUsageCache* usage_cache() { return file_system_usage_cache_.get(); }
  SandboxQuotaObserver* quota_observer() { return quota_observer_.get(); };

  quota::SpecialStoragePolicy* special_storage_policy() {
    return special_storage_policy_.get();
  }

  FileSystemOptions file_system_options() { return file_system_options_; }

  ObfuscatedFileUtil* sync_file_util();

 private:
  friend class SandboxQuotaObserver;
  friend class SandboxFileSystemTestHelper;

  // Returns a path to the usage cache file.
  base::FilePath GetUsageCachePathForOriginAndType(
      const GURL& origin_url,
      FileSystemType type);

  // Returns a path to the usage cache file (static version).
  static base::FilePath GetUsageCachePathForOriginAndType(
      ObfuscatedFileUtil* sandbox_file_util,
      const GURL& origin_url,
      FileSystemType type,
      base::PlatformFileError* error_out);

  int64 RecalculateUsage(FileSystemContext* context,
                         const GURL& origin,
                         FileSystemType type);

  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  scoped_ptr<AsyncFileUtilAdapter> sandbox_file_util_;
  scoped_ptr<FileSystemUsageCache> file_system_usage_cache_;
  scoped_ptr<SandboxQuotaObserver> quota_observer_;

  scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy_;

  FileSystemOptions file_system_options_;

  // Acccessed only on the file thread.
  std::set<GURL> visited_origins_;

  std::set<std::pair<GURL, FileSystemType> > sticky_dirty_origins_;

  base::Time next_release_time_for_open_filesystem_stat_;

  base::WeakPtrFactory<SandboxContext> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SandboxContext);
};

}  // namespace fileapi

#endif  // WEBKIT_BROWSER_FILEAPI_SANDBOX_CONTEXT_H_
