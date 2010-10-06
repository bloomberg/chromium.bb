// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/blob/deletable_file_reference.h"

#include <map>
#include "base/file_util.h"
#include "base/file_util_proxy.h"
#include "base/message_loop_proxy.h"
#include "base/singleton.h"

namespace webkit_blob {

namespace {

typedef std::map<FilePath, DeletableFileReference*> DeleteableFileMap;

DeleteableFileMap* map() {
  return Singleton<DeleteableFileMap>::get();
}

}  // namespace

// static
scoped_refptr<DeletableFileReference> DeletableFileReference::Get(
    const FilePath& path) {
  DeleteableFileMap::iterator found = map()->find(path);
  DeletableFileReference* reference =
      (found == map()->end()) ? NULL : found->second;
  return scoped_refptr<DeletableFileReference>(reference);
}

// static
scoped_refptr<DeletableFileReference> DeletableFileReference::GetOrCreate(
    const FilePath& path, base::MessageLoopProxy* file_thread) {
  DCHECK(file_thread);
  typedef std::pair<DeleteableFileMap::iterator, bool> InsertResult;
  InsertResult result = map()->insert(
      DeleteableFileMap::value_type(path, NULL));
  if (result.second == false)
    return scoped_refptr<DeletableFileReference>(result.first->second);

  // Wasn't in the map, create a new reference and store the pointer.
  scoped_refptr<DeletableFileReference> reference =
      new DeletableFileReference(path, file_thread);
  result.first->second = reference.get();
  return reference;
}

DeletableFileReference::DeletableFileReference(
    const FilePath& path, base::MessageLoopProxy* file_thread)
    : path_(path), file_thread_(file_thread) {
  DCHECK(map()->find(path_)->second == NULL);
}

DeletableFileReference::~DeletableFileReference() {
  DCHECK(map()->find(path_)->second == this);
  map()->erase(path_);
  base::FileUtilProxy::Delete(file_thread_, path_, NULL);
}

}  // namespace webkit_blob
