// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_UTIL_GET_SESSION_NAME_H_
#define SYNC_UTIL_GET_SESSION_NAME_H_

#include <string>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "sync/base/sync_export.h"

namespace base {
class TaskRunner;
}  // namespace base

namespace syncer {

// Invokes |done_callback| with the session name, a UTF-8 string.
SYNC_EXPORT void GetSessionName(
    const scoped_refptr<base::TaskRunner>& task_runner,
    const base::Callback<void(const std::string&)>& done_callback);

SYNC_EXPORT_PRIVATE std::string GetSessionNameSynchronouslyForTesting();

}  // namespace syncer

#endif  // SYNC_UTIL_GET_SESSION_NAME_H_
