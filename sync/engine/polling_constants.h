// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Constants used by SyncerThread when polling servers for updates.

#ifndef SYNC_ENGINE_POLLING_CONSTANTS_H_
#define SYNC_ENGINE_POLLING_CONSTANTS_H_
#pragma once

namespace browser_sync {

extern const int64 kDefaultShortPollIntervalSeconds;
extern const int64 kDefaultLongPollIntervalSeconds;
extern const int64 kMaxBackoffSeconds;
extern const int kBackoffRandomizationFactor;

}  // namespace browser_sync

#endif  // SYNC_ENGINE_POLLING_CONSTANTS_H_
