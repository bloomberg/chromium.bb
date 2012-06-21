// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_TEST_FAKE_EXTENSIONS_ACTIVITY_MONITOR_H_
#define SYNC_TEST_FAKE_EXTENSIONS_ACTIVITY_MONITOR_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/threading/non_thread_safe.h"
#include "sync/util/extensions_activity_monitor.h"

namespace csync {

// Fake non-thread-safe implementation of ExtensionsActivityMonitor
// suitable to be used in single-threaded sync tests.
class FakeExtensionsActivityMonitor
    : public ExtensionsActivityMonitor,
      public base::NonThreadSafe {
 public:
  FakeExtensionsActivityMonitor();
  virtual ~FakeExtensionsActivityMonitor();

  // ExtensionsActivityMonitor implementation.
  virtual void GetAndClearRecords(Records* buffer) OVERRIDE;
  virtual void PutRecords(const Records& records) OVERRIDE;

 private:
  Records records_;
};

}  // namespace csync

#endif  // SYNC_TEST_FAKE_EXTENSIONS_ACTIVITY_MONITOR_H_
