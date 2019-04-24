// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_CLOUD_MESSAGE_UTIL_H_
#define COMPONENTS_POLICY_CORE_BROWSER_CLOUD_MESSAGE_UTIL_H_

#include "base/strings/string16.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "components/policy/policy_export.h"

namespace policy {

// Returns a string describing |status| suitable for display in UI.
POLICY_EXPORT base::string16 FormatDeviceManagementStatus(
    DeviceManagementStatus status);

// Returns a string describing |validation_status| suitable for display in UI.
POLICY_EXPORT base::string16 FormatValidationStatus(
    CloudPolicyValidatorBase::Status validation_status);

// Returns a textual description of |store_status| for display in the UI. If
// |store_status| is STATUS_VALIDATION_FAILED, |validation_status| will be
// consulted to create a description of the validation failure.
POLICY_EXPORT base::string16 FormatStoreStatus(
    CloudPolicyStore::Status store_status,
    CloudPolicyValidatorBase::Status validation_status);

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_CLOUD_MESSAGE_UTIL_H_
