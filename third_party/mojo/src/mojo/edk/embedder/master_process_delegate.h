// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_EMBEDDER_MASTER_PROCESS_DELEGATE_H_
#define MOJO_EDK_EMBEDDER_MASTER_PROCESS_DELEGATE_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/edk/system/system_impl_export.h"

namespace mojo {
namespace embedder {

// An interface for containers of slave process information, to be used by
// |MasterProcessDelegate| (below).
class MOJO_SYSTEM_IMPL_EXPORT SlaveInfo {
 public:
  SlaveInfo() {}
  virtual ~SlaveInfo() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SlaveInfo);
};

// An interface for the master process delegate (which lives in the master
// process).
class MOJO_SYSTEM_IMPL_EXPORT MasterProcessDelegate {
 public:
  // Called when contact with the slave process specified by |slave_info| has
  // been lost.
  // TODO(vtl): Obviously, there needs to be a suitable embedder API for
  // connecting to a process. What will it be? Mention that here once it exists.
  virtual void OnSlaveDisconnect(scoped_ptr<SlaveInfo> slave_info) = 0;

 protected:
  MasterProcessDelegate() {}
  virtual ~MasterProcessDelegate() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MasterProcessDelegate);
};

}  // namespace embedder
}  // namespace mojo

#endif  // MOJO_EDK_EMBEDDER_MASTER_PROCESS_DELEGATE_H_
