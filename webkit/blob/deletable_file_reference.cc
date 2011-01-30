// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/blob/deletable_file_reference.h"

#include <map>
#include "base/file_util.h"
#include "base/file_util_proxy.h"
#include "base/lazy_instance.h"
#include "base/message_loop_proxy.h"

namespace webkit_blob {

namespace {

typedef std::map<FilePath, DeletableFileReference*> DeleteableFileMap;
static base::LazyInstance<DeleteableFileMap> g_deletable_file_map(
    base::LINKER_INITIALIZED);

}  // namespace

// static
scoped_refptr<DeletableFileReference> DeletableFileReference::Get(
    const FilePath& path) {
  DeleteableFileMap::iterator found = g_deletable_file_map.Get().find(path);
  DeletableFileReference* reference =
      (found == g_deletable_file_map.Get().end()) ? NULL : found->second;
  return scoped_refptr<DeletableFileReference>(reference);
}

// static
scoped_refptr<DeletableFileReference> DeletableFileReference::GetOrCreate(
    const FilePath& path, base::MessageLoopProxy* file_thread) {
  DCHECK(file_thread);
  typedef std::pair<DeleteableFileMap::iterator, bool> InsertResult;

  // Visual Studio 2010 has problems converting NULL to the null pointer for
  // std::pair.  See http://connect.microsoft.com/VisualStudio/feedback/details/520043/error-converting-from-null-to-a-pointer-type-in-std-pair
  // It will work if we pass nullptr.
#if defined(_MSC_VER) && _MSC_VER >= 1600
  webkit_blob::DeletableFileReference* null_reference = nullptr;
#else
  webkit_blob::DeletableFileReference* null_reference = NULL;
#endif
  InsertResult result = g_deletable_file_map.Get().insert(
      DeleteableFileMap::value_type(path, null_reference));
  if (result.second == false)
    return scoped_refptr<DeletableFileReference>(result.first->second);

  // Wasn't in the map, create a new reference and store the pointer.
  scoped_refptr<DeletableFileReference> reference(
      new DeletableFileReference(path, file_thread));
  result.first->second = reference.get();
  return reference;
}

DeletableFileReference::DeletableFileReference(
    const FilePath& path, base::MessageLoopProxy* file_thread)
    : path_(path), file_thread_(file_thread) {
  DCHECK(g_deletable_file_map.Get().find(path_)->second == NULL);
}

DeletableFileReference::~DeletableFileReference() {
  DCHECK(g_deletable_file_map.Get().find(path_)->second == this);
  g_deletable_file_map.Get().erase(path_);
  base::FileUtilProxy::Delete(file_thread_, path_, false /* recursive */, NULL);
}

}  // namespace webkit_blob
