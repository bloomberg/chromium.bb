// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_MOJO_MOJO_VFS_H_
#define SQL_MOJO_MOJO_VFS_H_

#include "base/macros.h"
#include "components/filesystem/public/interfaces/directory.mojom.h"

typedef struct sqlite3_vfs sqlite3_vfs;

namespace sql {

// Changes the default sqlite3 vfs to a vfs that uses proxies calls to the
// mojo:filesystem service. Instantiating this object transparently changes how
// the entire //sql/ subsystem works in the process of the caller; all paths
// are treated as relative to |directory|.
class ScopedMojoFilesystemVFS {
 public:
  explicit ScopedMojoFilesystemVFS(filesystem::DirectoryPtr directory);
  ~ScopedMojoFilesystemVFS();

  // Returns the directory of the current VFS.
  filesystem::DirectoryPtr& GetDirectory();

 private:
  friend sqlite3_vfs* GetParentVFS(sqlite3_vfs* mojo_vfs);
  friend filesystem::DirectoryPtr& GetRootDirectory(sqlite3_vfs* mojo_vfs);

  // The default vfs at the time MojoVFS was installed. We use the to pass
  // through things like randomness requests and per-platform sleep calls.
  sqlite3_vfs* parent_;

  // When we initialize the subsystem, we are given a filesystem::Directory
  // object, which is the root directory of a mojo:filesystem. All access to
  // various files are specified from this root directory.
  filesystem::DirectoryPtr root_directory_;

  DISALLOW_COPY_AND_ASSIGN(ScopedMojoFilesystemVFS);
};

}  // namespace sql

#endif  // SQL_MOJO_MOJO_VFS_H_
