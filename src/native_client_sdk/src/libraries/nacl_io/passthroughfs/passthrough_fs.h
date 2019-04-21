// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_PASSTHROUGHFS_PASSTHROUGH_FS_H_
#define LIBRARIES_NACL_IO_PASSTHROUGHFS_PASSTHROUGH_FS_H_

#include "nacl_io/filesystem.h"
#include "nacl_io/typed_fs_factory.h"

namespace nacl_io {

class PassthroughFs : public Filesystem {
 protected:
  PassthroughFs();

  virtual Error Init(const FsInitArgs& args);
  virtual void Destroy();

 public:
  virtual Error OpenWithMode(const Path& path, int open_flags, mode_t mode,
                             ScopedNode* out_node);
  virtual Error OpenResource(const Path& path, ScopedNode* out_node);
  virtual Error Unlink(const Path& path);
  virtual Error Mkdir(const Path& path, int perm);
  virtual Error Rmdir(const Path& path);
  virtual Error Remove(const Path& path);
  virtual Error Rename(const Path& path, const Path& newpath);

 private:
  friend class TypedFsFactory<PassthroughFs>;
  DISALLOW_COPY_AND_ASSIGN(PassthroughFs);
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_PASSTHROUGHFS_PASSTHROUGH_FS_H_
