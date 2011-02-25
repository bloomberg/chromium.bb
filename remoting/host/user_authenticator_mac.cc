// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/user_authenticator_fake.h"

namespace remoting {

// static
UserAuthenticator* UserAuthenticator::Create() {
  return new UserAuthenticatorFake();
}

}  // namespace remoting
