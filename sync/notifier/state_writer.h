// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Simple interface for something that persists state.

#ifndef SYNC_NOTIFIER_STATE_WRITER_H_
#define SYNC_NOTIFIER_STATE_WRITER_H_

#include <string>

#include "sync/base/sync_export.h"

namespace syncer {

class SYNC_EXPORT_PRIVATE StateWriter {
 public:
  virtual ~StateWriter() {}

  virtual void WriteState(const std::string& state) = 0;
};

}  // namespace syncer

#endif  // SYNC_NOTIFIER_STATE_WRITER_H_
