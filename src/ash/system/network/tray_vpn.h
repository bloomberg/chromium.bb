// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_TRAY_VPN_H_
#define ASH_SYSTEM_NETWORK_TRAY_VPN_H_

namespace ash {
namespace tray {

// TODO(tetsui): Move them to VpnList.  https://crbug.com/901714
extern bool IsVPNVisibleInSystemTray();
extern bool IsVPNEnabled();
extern bool IsVPNConnected();

}  // namespace tray
}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_TRAY_VPN_H_
