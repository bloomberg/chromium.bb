// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_OS_CRYPT_PREF_NAMES_MAC_H_
#define COMPONENTS_OS_CRYPT_OS_CRYPT_PREF_NAMES_MAC_H_

#include "base/component_export.h"

namespace os_crypt {
namespace prefs {

// The boolean which indicates the existence of the encryption key in the
// Keychain.
// Sometimes when the Keychain seems to be available, it may happen that Chrome
// fails to retrieve the key from the Keychain, which causes Chrome to overwrite
// the old key with a newly generated key. Overwriting the encryption key can
// cause various problems. This flag should be set to true once the
// encryption key is generated or successfully retrieved. If this flag is set to
// true and Chrome couldn't get the encryption key from the Keychain, it signals
// that something is going wrong on the machine.
COMPONENT_EXPORT(OS_CRYPT) extern const char kKeyCreated[];

}  // namespace prefs
}  // namespace os_crypt

#endif  // COMPONENTS_OS_CRYPT_OS_CRYPT_PREF_NAMES_MAC_H_
