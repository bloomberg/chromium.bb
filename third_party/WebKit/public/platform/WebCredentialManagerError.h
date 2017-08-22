// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebCredentialManagerError_h
#define WebCredentialManagerError_h

namespace blink {

// FIXME: This is a placeholder list of error conditions. We'll likely expand
// the list as the API evolves.
enum WebCredentialManagerError {
  kWebCredentialManagerNoError = 0,
  kWebCredentialManagerDisabledError,
  kWebCredentialManagerPendingRequestError,
  kWebCredentialManagerPasswordStoreUnavailableError,
  kWebCredentialManagerNotAllowedError,
  kWebCredentialManagerNotSupportedError,
  kWebCredentialManagerSecurityError,
  kWebCredentialManagerCancelledError,
  kWebCredentialManagerNotImplementedError,
  kWebCredentialManagerUnknownError,
  kWebCredentialManagerErrorLastType = kWebCredentialManagerUnknownError,
};

}  // namespace blink

#endif
