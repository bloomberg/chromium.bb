// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SERVICES_NAMESPACE_UTILS_H_
#define SANDBOX_LINUX_SERVICES_NAMESPACE_UTILS_H_

#include <sys/types.h>

#include "base/macros.h"
#include "base/template_util.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {

// Utility functions for using Linux namepaces.
class SANDBOX_EXPORT NamespaceUtils {
 public:
  COMPILE_ASSERT((base::is_same<uid_t, gid_t>::value), UidAndGidAreSameType);
  // generic_id_t can be used for either uid_t or gid_t.
  typedef uid_t generic_id_t;

  // Write a uid or gid mapping from |id| to |id| in |map_file|.
  static bool WriteToIdMapFile(const char* map_file, generic_id_t id);

  // Returns true if unprivileged namespaces of type |type| is supported
  // (meaning that both CLONE_NEWUSER and type are are supported).  |type| must
  // be one of CLONE_NEWIPC, CLONE_NEWNET, CLONE_NEWNS, CLONE_NEWPID,
  // CLONE_NEWUSER, or CLONE_NEWUTS. This relies on access to /proc, so it will
  // not work from within a sandbox.
  static bool KernelSupportsUnprivilegedNamespace(int type);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(NamespaceUtils);
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SERVICES_NAMESPACE_UTILS_H_
