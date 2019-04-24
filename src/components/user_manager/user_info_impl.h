// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_USER_INFO_IMPL_H_
#define COMPONENTS_USER_MANAGER_USER_INFO_IMPL_H_

#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_info.h"
#include "components/user_manager/user_manager_export.h"
#include "ui/gfx/image/image_skia.h"

namespace user_manager {

// Stub implementation of UserInfo interface. Used in tests.
class USER_MANAGER_EXPORT UserInfoImpl : public UserInfo {
 public:
  UserInfoImpl();
  ~UserInfoImpl() override;

  // UserInfo:
  base::string16 GetDisplayName() const override;
  base::string16 GetGivenName() const override;
  std::string GetDisplayEmail() const override;
  const AccountId& GetAccountId() const override;
  const gfx::ImageSkia& GetImage() const override;

 private:
  const AccountId account_id_;
  gfx::ImageSkia user_image_;

  DISALLOW_COPY_AND_ASSIGN(UserInfoImpl);
};

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_USER_INFO_IMPL_H_
