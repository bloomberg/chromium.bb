// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/util/weak_handle.h"

#include <sstream>

#include "base/callback.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"

namespace syncer {

namespace internal {

WeakHandleCoreBase::WeakHandleCoreBase()
    : owner_loop_proxy_(base::MessageLoopProxy::current()) {}

bool WeakHandleCoreBase::IsOnOwnerThread() const {
  return owner_loop_proxy_->BelongsToCurrentThread();
}

WeakHandleCoreBase::~WeakHandleCoreBase() {}

void WeakHandleCoreBase::PostToOwnerThread(
    const tracked_objects::Location& from_here,
    const base::Closure& fn) const {
  if (!owner_loop_proxy_->PostTask(from_here, fn)) {
    DVLOG(1) << "Could not post task from " << from_here.ToString();
  }
}

}  // namespace internal

}  // namespace syncer
