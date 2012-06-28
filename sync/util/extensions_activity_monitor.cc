// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/util/extensions_activity_monitor.h"

namespace syncer {

ExtensionsActivityMonitor::Record::Record()
    : bookmark_write_count(0U) {}

ExtensionsActivityMonitor::Record::~Record() {}

ExtensionsActivityMonitor::~ExtensionsActivityMonitor() {}

}  // namespace syncer
