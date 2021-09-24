// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The reason a page was closed. Keep in sync with NearbyShareDialogUI.
 * @enum {number}
 */
export const CloseReason = {
  UNKNOWN: 0,
  TRANSFER_STARTED: 1,
  TRANSFER_SUCCEEDED: 2,
  CANCELLED: 3,
  REJECTED: 4,
};
