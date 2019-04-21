// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_CONSTANTS_H_
#define CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_CONSTANTS_H_

namespace chromeos {
namespace smb_client {

extern const char kSmbScheme[];
extern const char kSmbSchemePrefix[];

constexpr int kNetBiosDiscoveryTimeoutSeconds = 1;

}  // namespace smb_client
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_CONSTANTS_H_
