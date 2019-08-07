// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/login_shelf_test_helper.h"

#include "ash/public/interfaces/constants.mojom.h"
#include "ash/public/interfaces/login_screen_test_api.test-mojom-test-utils.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"

namespace chromeos {

LoginShelfTestHelper::LoginShelfTestHelper() {
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName, &login_screen_);
}

LoginShelfTestHelper::~LoginShelfTestHelper() = default;

bool LoginShelfTestHelper::IsLoginShelfShown() {
  ash::mojom::LoginScreenTestApiAsyncWaiter waiter(login_screen_.get());
  bool is_shown;
  waiter.IsLoginShelfShown(&is_shown);
  return is_shown;
}

}  // namespace chromeos
