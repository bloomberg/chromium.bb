// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_UTIL_ACCOUNT_IDENTIFIER_OPERATORS_H_
#define CHROMEOS_DBUS_UTIL_ACCOUNT_IDENTIFIER_OPERATORS_H_

#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"

namespace cryptohome {

// operator< to use AccountIdentifier in STL containers.
CHROMEOS_EXPORT bool operator<(const AccountIdentifier& l,
                               const AccountIdentifier& r);

// operator== to use AccountIdentifier in tests.
CHROMEOS_EXPORT bool operator==(const AccountIdentifier& l,
                                const AccountIdentifier& r);

}  // namespace cryptohome

#endif  // CHROMEOS_DBUS_UTIL_ACCOUNT_IDENTIFIER_OPERATORS_H_
