// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/credentialmanager/PlatformCredential.h"

namespace blink {

PlatformCredential* PlatformCredential::Create(const String& id) {
  return new PlatformCredential(id);
}

PlatformCredential::PlatformCredential(const String& id)
    : id_(id), type_("credential") {}

PlatformCredential::~PlatformCredential() {}

}  // namespace blink
