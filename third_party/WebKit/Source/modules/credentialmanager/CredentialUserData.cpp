// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/credentialmanager/CredentialUserData.h"

namespace blink {

CredentialUserData::CredentialUserData(PlatformCredential* platform_credential)
    : Credential(platform_credential) {}

}  // namespace blink
