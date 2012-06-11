// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/fake_extensions_activity_monitor.h"

#include "base/logging.h"

namespace browser_sync {

FakeExtensionsActivityMonitor::FakeExtensionsActivityMonitor() {}

FakeExtensionsActivityMonitor::~FakeExtensionsActivityMonitor() {
  DCHECK(CalledOnValidThread());
}

void FakeExtensionsActivityMonitor::GetAndClearRecords(Records* buffer) {
  DCHECK(CalledOnValidThread());
  buffer->clear();
  buffer->swap(records_);
}

void FakeExtensionsActivityMonitor::PutRecords(const Records& records) {
  DCHECK(CalledOnValidThread());
  for (Records::const_iterator i = records.begin(); i != records.end(); ++i) {
    records_[i->first].extension_id = i->second.extension_id;
    records_[i->first].bookmark_write_count += i->second.bookmark_write_count;
  }
}

}  // namespace browser_sync
