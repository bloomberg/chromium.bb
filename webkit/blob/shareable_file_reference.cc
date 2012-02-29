// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/blob/shareable_file_reference.h"

#include <map>
#include "base/file_util.h"
#include "base/file_util_proxy.h"
#include "base/lazy_instance.h"
#include "base/message_loop_proxy.h"

namespace webkit_blob {

namespace {

typedef std::map<FilePath, ShareableFileReference*> ShareableFileMap;
base::LazyInstance<ShareableFileMap> g_file_map = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
scoped_refptr<ShareableFileReference> ShareableFileReference::Get(
    const FilePath& path) {
  ShareableFileMap::iterator found = g_file_map.Get().find(path);
  ShareableFileReference* reference =
      (found == g_file_map.Get().end()) ? NULL : found->second;
  return scoped_refptr<ShareableFileReference>(reference);
}

// static
scoped_refptr<ShareableFileReference> ShareableFileReference::GetOrCreate(
    const FilePath& path, FinalReleasePolicy policy,
    base::MessageLoopProxy* file_thread) {
  DCHECK(file_thread);
  typedef std::pair<ShareableFileMap::iterator, bool> InsertResult;

  // Required for VS2010: http://connect.microsoft.com/VisualStudio/feedback/details/520043/error-converting-from-null-to-a-pointer-type-in-std-pair
  webkit_blob::ShareableFileReference* null_reference = NULL;
  InsertResult result = g_file_map.Get().insert(
      ShareableFileMap::value_type(path, null_reference));
  if (result.second == false)
    return scoped_refptr<ShareableFileReference>(result.first->second);

  // Wasn't in the map, create a new reference and store the pointer.
  scoped_refptr<ShareableFileReference> reference(
      new ShareableFileReference(path, policy, file_thread));
  result.first->second = reference.get();
  return reference;
}

void ShareableFileReference::AddFinalReleaseCallback(
    const FinalReleaseCallback& callback) {
  final_release_callbacks_.push_back(callback);
}

ShareableFileReference::ShareableFileReference(
    const FilePath& path, FinalReleasePolicy policy,
    base::MessageLoopProxy* file_thread)
    : path_(path), final_release_policy_(policy), file_thread_(file_thread) {
  DCHECK(g_file_map.Get().find(path_)->second == NULL);
}

ShareableFileReference::~ShareableFileReference() {
  DCHECK(g_file_map.Get().find(path_)->second == this);
  g_file_map.Get().erase(path_);

  for (size_t i = 0; i < final_release_callbacks_.size(); i++)
    final_release_callbacks_[i].Run(path_);

  if (final_release_policy_ == DELETE_ON_FINAL_RELEASE) {
    base::FileUtilProxy::Delete(file_thread_, path_, false /* recursive */,
                                base::FileUtilProxy::StatusCallback());
  }
}

}  // namespace webkit_blob
