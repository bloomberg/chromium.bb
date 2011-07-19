// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/auth_token_util.h"
#include "remoting/base/constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace {

TEST(AuthTokenUtilTest, ParseAuthTokenWithService) {
  std::string auth_token;
  std::string auth_service;

  ParseAuthTokenWithService("service:token", &auth_token, &auth_service);
  EXPECT_EQ("token", auth_token);
  EXPECT_EQ("service", auth_service);

  // Check for legacy support.
  ParseAuthTokenWithService("token2", &auth_token, &auth_service);
  EXPECT_EQ("token2", auth_token);
  EXPECT_EQ(std::string(kChromotingTokenDefaultServiceName), auth_service);

  ParseAuthTokenWithService("just_service:", &auth_token, &auth_service);
  EXPECT_EQ("", auth_token);
  EXPECT_EQ("just_service", auth_service);

  ParseAuthTokenWithService("yay:token:has:colons", &auth_token, &auth_service);
  EXPECT_EQ("token:has:colons", auth_token);
  EXPECT_EQ("yay", auth_service);
}

}  // namespace

}  // namespace remoting
