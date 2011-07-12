/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_DEV_PPP_NETWORK_STATE_DEV_H_
#define PPAPI_C_DEV_PPP_NETWORK_STATE_DEV_H_

/**
 * @file
 * This file defines the PPP_NetworkState interface.
 */

#define PPP_NETWORK_STATE_DEV_INTERFACE "PPP_NetworkState(Dev);0.1"

struct PPP_NetworkState_Dev {
  /**
   * Notification that the online state has changed for the user's network.
   * This will change as a result of a network cable being plugged or
   * unplugged, WiFi connections going up and down, or other events.
   *
   * Note that being "online" isn't a guarantee that any particular connections
   * will succeed.
   */
  void (*SetOnLine)(PP_Bool is_online);
};

#endif  // PPAPI_C_DEV_PPP_NETWORK_STATE_DEV_H_
