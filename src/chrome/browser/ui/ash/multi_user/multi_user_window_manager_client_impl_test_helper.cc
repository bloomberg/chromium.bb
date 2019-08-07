// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_client_impl_test_helper.h"

#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_client_impl.h"
#include "ui/base/ui_base_features.h"

// static
void MultiUserWindowManagerClientImplTestHelper::FlushBindings() {
  MultiUserWindowManagerClientImpl* instance =
      MultiUserWindowManagerClientImpl::instance_;
  if (!instance)
    return;
  if (features::IsUsingWindowService())
    instance->client_binding_.FlushForTesting();
  else
    instance->classic_support_->binding.FlushForTesting();
}
