// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/federated_identity_api_permission_context_delegate.h"

namespace content {

bool FederatedIdentityApiPermissionContextDelegate::
    ShouldCompleteRequestImmediatelyOnError() const {
  return false;
}

}  // namespace content
