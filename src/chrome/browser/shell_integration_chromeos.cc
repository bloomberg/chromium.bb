// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_integration.h"

namespace shell_integration {

bool SetAsDefaultBrowser() {
  return false;
}

bool SetAsDefaultProtocolClient(const std::string& protocol) {
  return false;
}

DefaultWebClientSetPermission GetDefaultWebClientSetPermission() {
  return SET_DEFAULT_NOT_ALLOWED;
}

base::string16 GetApplicationNameForProtocol(const GURL& url) {
  return base::string16();
}

DefaultWebClientState GetDefaultBrowser() {
  return UNKNOWN_DEFAULT;
}

bool IsFirefoxDefaultBrowser() {
  return false;
}

DefaultWebClientState IsDefaultProtocolClient(const std::string& protocol) {
  return UNKNOWN_DEFAULT;
}

}  // namespace shell_integration
