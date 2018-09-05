// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_CATALOG_SERVICE_OPTIONS_H_
#define SERVICES_CATALOG_SERVICE_OPTIONS_H_

namespace catalog {

struct ServiceOptions {
  enum class InstanceSharingType {
    NONE,
    SINGLETON,
    SHARED_INSTANCE_ACROSS_USERS
  };

  InstanceSharingType instance_sharing = InstanceSharingType::NONE;
  bool can_connect_to_other_services_as_any_user = false;
  bool allow_other_instance_names = false;
  bool instance_for_client_process = false;
};

}  // namespace catalog

#endif  // SERVICES_CATALOG_SERVICE_OPTIONS_H_