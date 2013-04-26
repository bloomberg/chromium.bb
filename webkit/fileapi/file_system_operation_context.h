// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_OPERATION_CONTEXT_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_OPERATION_CONTEXT_H_

#include "base/supports_user_data.h"
#include "webkit/fileapi/task_runner_bound_observer_list.h"
#include "webkit/quota/quota_types.h"
#include "webkit/storage/webkit_storage_export.h"

namespace base {
class SequencedTaskRunner;
}

namespace fileapi {

class FileSystemContext;

// A context class which is carried around by FileSystemOperation and
// its delegated tasks. It is valid to reuse one context instance across
// multiple operations as far as those operations are supposed to share
// the same context (e.g. use the same task runner, share the quota etc).
// Note that the remaining quota bytes (allowed_bytes_growth) may be
// updated during the execution of write operations.
class WEBKIT_STORAGE_EXPORT_PRIVATE FileSystemOperationContext
    : public base::SupportsUserData {
 public:
  explicit FileSystemOperationContext(FileSystemContext* context);

  // Specifies |task_runner| which the operation is performed on.
  FileSystemOperationContext(FileSystemContext* context,
                             base::SequencedTaskRunner* task_runner);

  virtual ~FileSystemOperationContext();

  FileSystemContext* file_system_context() const {
    return file_system_context_;
  }

  // Updates the current remaining quota.
  void set_allowed_bytes_growth(const int64& allowed_bytes_growth) {
    allowed_bytes_growth_ = allowed_bytes_growth;
  }

  // Returns the current remaining quota.
  int64 allowed_bytes_growth() const { return allowed_bytes_growth_; }

  quota::QuotaLimitType quota_limit_type() const {
    return quota_limit_type_;
  }

  // Returns TaskRunner which the operation is performed on.
  base::SequencedTaskRunner* task_runner() const {
    return task_runner_.get();
  }

  ChangeObserverList* change_observers() { return &change_observers_; }
  AccessObserverList* access_observers() { return &access_observers_; }
  UpdateObserverList* update_observers() { return &update_observers_; }

  // Gets and sets value-type (or not-owned) variable as UserData.
  template <typename T> T GetUserValue(const char* key) const {
    ValueAdapter<T>* v = static_cast<ValueAdapter<T>*>(GetUserData(key));
    return v ? v->value() : T();
  }
  template <typename T> void SetUserValue(const char* key, const T& value) {
    SetUserData(key, new ValueAdapter<T>(value));
  }

 private:
  // An adapter for setting a value-type (or not owned) data as UserData.
  template <typename T> class ValueAdapter
      : public base::SupportsUserData::Data {
   public:
    ValueAdapter(const T& value) : value_(value) {}
    const T& value() const { return value_; }
   private:
    T value_;
    DISALLOW_COPY_AND_ASSIGN(ValueAdapter);
  };

  // Only regular (and test) MountPointProviders can access following setters.
  friend class IsolatedMountPointProvider;
  friend class SandboxMountPointProvider;
  friend class TestMountPointProvider;

  // Tests also need access to some setters.
  friend class FileSystemQuotaClientTest;
  friend class LocalFileSystemOperationTest;
  friend class LocalFileSystemOperationWriteTest;
  friend class LocalFileSystemTestOriginHelper;
  friend class ObfuscatedFileUtilTest;

  void set_change_observers(const ChangeObserverList& list) {
    change_observers_ = list;
  }
  void set_access_observers(const AccessObserverList& list) {
    access_observers_ = list;
  }
  void set_update_observers(const UpdateObserverList& list) {
    update_observers_ = list;
  }
  void set_quota_limit_type(quota::QuotaLimitType limit_type) {
    quota_limit_type_ = limit_type;
  }

  FileSystemContext* file_system_context_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  int64 allowed_bytes_growth_;
  quota::QuotaLimitType quota_limit_type_;

  AccessObserverList access_observers_;
  ChangeObserverList change_observers_;
  UpdateObserverList update_observers_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemOperationContext);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_OPERATION_CONTEXT_H_
