// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth2_access_token_consumer.h"

OAuth2AccessTokenConsumer::TokenResponse::TokenResponse(
    const std::string& access_token,
    const base::Time& expiration_time,
    const std::string& id_token)
    : access_token(access_token),
      expiration_time(expiration_time),
      id_token(id_token) {}

OAuth2AccessTokenConsumer::~OAuth2AccessTokenConsumer() {}

void OAuth2AccessTokenConsumer::OnGetTokenSuccess(
    const TokenResponse& token_response) {}

void OAuth2AccessTokenConsumer::OnGetTokenFailure(
    const GoogleServiceAuthError& error) {}