// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_DRIVE_AUTH_SERVICE_INTERFACE_H_
#define GOOGLE_APIS_DRIVE_AUTH_SERVICE_INTERFACE_H_

#include <string>

#include "base/callback_forward.h"
#include "google_apis/drive/drive_api_error_codes.h"

namespace google_apis {

class AuthServiceObserver;

// Called when fetching of access token is complete.
typedef base::Callback<void(DriveApiErrorCode error,
                            const std::string& access_token)>
    AuthStatusCallback;

// This defines an interface for the authentication service which is required
// by authenticated requests (AuthenticatedRequestInterface).
// All functions must be called on UI thread.
class AuthServiceInterface {
 public:
  virtual ~AuthServiceInterface() {}

  // Adds and removes the observer.
  virtual void AddObserver(AuthServiceObserver* observer) = 0;
  virtual void RemoveObserver(AuthServiceObserver* observer) = 0;

  // Starts fetching OAuth2 access token from the refresh token.
  // |callback| must not be null.
  virtual void StartAuthentication(const AuthStatusCallback& callback) = 0;

  // True if an OAuth2 access token is retrieved and believed to be fresh.
  // The access token is used to access the Drive server.
  virtual bool HasAccessToken() const = 0;

  // True if an OAuth2 refresh token is present. Its absence means that user
  // is not properly authenticated.
  // The refresh token is used to get the access token.
  virtual bool HasRefreshToken() const = 0;

  // Returns OAuth2 access token.
  virtual const std::string& access_token() const = 0;

  // Clears OAuth2 access token.
  virtual void ClearAccessToken() = 0;

  // Clears OAuth2 refresh token.
  virtual void ClearRefreshToken() = 0;
};

}  // namespace google_apis

#endif  // GOOGLE_APIS_DRIVE_AUTH_SERVICE_INTERFACE_H_
