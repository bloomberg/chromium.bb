// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/syncable/scoped_kernel_lock.h"

#include "components/sync/syncable/directory.h"

namespace syncer {
namespace syncable {

ScopedKernelLock::ScopedKernelLock(const Directory* dir)
    : scoped_lock_(dir->kernel()->mutex), dir_(dir) {}

ScopedKernelLock::~ScopedKernelLock() {}

}  // namespace syncable
}  // namespace syncer
