// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_PLATFORM_MANAGEMENT_SERVICE_H_
#define COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_PLATFORM_MANAGEMENT_SERVICE_H_

#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/policy_export.h"

namespace policy {

// This class gives information related to the OS' management state.
class POLICY_EXPORT PlatformManagementService : public ManagementService {
 public:
  PlatformManagementService();
  ~PlatformManagementService() override;

  static PlatformManagementService& GetInstance();

 protected:
  // ManagementService impl
  void InitManagementStatusProviders() override;
};

}  // namespace policy

#endif  // #define
        // COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_PLATFOR_MANAGEMENT_SERVICE_H_
