// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_CLEAR_SERVER_DATA_EVENTS_H_
#define COMPONENTS_SYNC_DRIVER_CLEAR_SERVER_DATA_EVENTS_H_

namespace syncer {

// Events in ClearServerData flow to be recorded in histogram. Existing
// constants should not be deleted or reordered. New ones shold be added at the
// end, before CLEAR_SERVER_DATA_MAX.
enum ClearServerDataEvents {
  // ClearServerData started after user switched to custom passphrase.
  CLEAR_SERVER_DATA_STARTED,
  // DataTypeManager reported that catchup configuration failed.
  CLEAR_SERVER_DATA_CATCHUP_FAILED,
  // ClearServerData flow restarted after browser restart.
  CLEAR_SERVER_DATA_RETRIED,
  // Success.
  CLEAR_SERVER_DATA_SUCCEEDED,
  // Client received RECET_LOCAL_SYNC_DATA after custom passphrase was enabled
  // on different client.
  CLEAR_SERVER_DATA_RESET_LOCAL_DATA_RECEIVED,
  CLEAR_SERVER_DATA_MAX
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_CLEAR_SERVER_DATA_EVENTS_H_
