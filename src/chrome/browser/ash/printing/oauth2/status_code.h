// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_OAUTH2_STATUS_CODE_H_
#define CHROME_BROWSER_ASH_PRINTING_OAUTH2_STATUS_CODE_H_

#include <string>

#include "base/callback.h"

namespace ash {
namespace printing {
namespace oauth2 {

enum class StatusCode {
  // Success - no errors occurred.
  kOK = 0,
  // Required session already exists, so the authorization attempt was ignored.
  kAlreadyAuthorized,
  // The client is registered to the server but there is no active OAuth2
  // sessions. Run the method InitAuthorization(...) and then
  // FinishAuthorization(...) to start OAuth2 session,
  kAuthorizationNeeded,
  // The client is not registered to the server and the server does not support
  // dynamic registration (as described in rfc7591).
  kClientNotRegistered,
  // The server is unknown (not trusted).
  kUnknownAuthorizationServer,
  // The server denied the request.
  kAccessDenied,
  // Access token is invalid or expired (used internally only).
  kInvalidAccessToken,
  // The server sent 503 Service Unavailable HTTP status code.
  kServerTemporarilyUnavailable,
  // The server sent 500 Internal Server Error HTTP status code.
  kServerError,
  // The server sent invalid or unexpected response.
  kInvalidResponse,
  // Cannot open HTTPS connection to the server.
  kConnectionError,
  // Maximum number of pending authorizations was reached.
  kTooManyPendingAuthorizations,
  // Unexpected or unknown error occurred.
  kUnexpectedError
};

// This is the standard callback used in oauth2 namespace. When `status` equals
// StatusCode::kOK, `data` may contain an access token or authorization URL.
// When `status` is different than StatusCode::kOK, `data` may contain
// an additional error message.
using StatusCallback =
    base::OnceCallback<void(StatusCode status, const std::string& data)>;

}  // namespace oauth2
}  // namespace printing
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_OAUTH2_STATUS_CODE_H_
