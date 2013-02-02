// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/blob/shareable_file_reference.h"

#include <map>

#include "base/file_util.h"
#include "base/files/file_util_proxy.h"
#include "base/lazy_instance.h"
#include "base/task_runner.h"
#include "base/threading/thread_checker.h"

namespace webkit_blob {

namespace {

// A shareable file map with enforcement of thread checker.
// This map may get deleted on a different thread in AtExitManager at the
// very end on the main thread (at the point all other threads must be
// terminated), so we use ThreadChecker rather than NonThreadSafe and do not
// check thread in the dtor.
class ShareableFileMap {
 public:
  typedef std::map<base::FilePath, ShareableFileReference*> FileMap;
  typedef FileMap::iterator iterator;
  typedef FileMap::key_type key_type;
  typedef FileMap::value_type value_type;

  ShareableFileMap() {}

  iterator Find(key_type key) {
    DCHECK(CalledOnValidThread());
    return file_map_.find(key);
  }

  iterator End() {
    DCHECK(CalledOnValidThread());
    return file_map_.end();
  }

  std::pair<iterator, bool> Insert(value_type value) {
    DCHECK(CalledOnValidThread());
    return file_map_.insert(value);
  }

  void Erase(key_type key) {
    DCHECK(CalledOnValidThread());
    file_map_.erase(key);
  }

  bool CalledOnValidThread() const {
    return thread_checker_.CalledOnValidThread();
  }

 private:
  FileMap file_map_;
  base::ThreadChecker thread_checker_;
  DISALLOW_COPY_AND_ASSIGN(ShareableFileMap);
};

base::LazyInstance<ShareableFileMap> g_file_map = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
scoped_refptr<ShareableFileReference> ShareableFileReference::Get(
    const base::FilePath& path) {
  ShareableFileMap::iterator found = g_file_map.Get().Find(path);
  ShareableFileReference* reference =
      (found == g_file_map.Get().End()) ? NULL : found->second;
  return scoped_refptr<ShareableFileReference>(reference);
}

// static
scoped_refptr<ShareableFileReference> ShareableFileReference::GetOrCreate(
    const base::FilePath& path, FinalReleasePolicy policy,
    base::TaskRunner* file_task_runner) {
  DCHECK(file_task_runner);
  typedef std::pair<ShareableFileMap::iterator, bool> InsertResult;

  // Required for VS2010: http://connect.microsoft.com/VisualStudio/feedback/details/520043/error-converting-from-null-to-a-pointer-type-in-std-pair
  webkit_blob::ShareableFileReference* null_reference = NULL;
  InsertResult result = g_file_map.Get().Insert(
      ShareableFileMap::value_type(path, null_reference));
  if (result.second == false)
    return scoped_refptr<ShareableFileReference>(result.first->second);

  // Wasn't in the map, create a new reference and store the pointer.
  scoped_refptr<ShareableFileReference> reference(
      new ShareableFileReference(path, policy, file_task_runner));
  result.first->second = reference.get();
  return reference;
}

void ShareableFileReference::AddFinalReleaseCallback(
    const FinalReleaseCallback& callback) {
  DCHECK(g_file_map.Get().CalledOnValidThread());
  final_release_callbacks_.push_back(callback);
}

ShareableFileReference::ShareableFileReference(
    const base::FilePath& path, FinalReleasePolicy policy,
    base::TaskRunner* file_task_runner)
    : path_(path),
      final_release_policy_(policy),
      file_task_runner_(file_task_runner) {
  DCHECK(g_file_map.Get().Find(path_)->second == NULL);
}

ShareableFileReference::~ShareableFileReference() {
  DCHECK(g_file_map.Get().Find(path_)->second == this);
  g_file_map.Get().Erase(path_);

  for (size_t i = 0; i < final_release_callbacks_.size(); i++)
    final_release_callbacks_[i].Run(path_);

  if (final_release_policy_ == DELETE_ON_FINAL_RELEASE) {
    base::FileUtilProxy::Delete(file_task_runner_, path_, false /* recursive */,
                                base::FileUtilProxy::StatusCallback());
  }
}

}  // namespace webkit_blob
