// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_CATALOG_SERVICE_OPTIONS_H_
#define SERVICES_CATALOG_SERVICE_OPTIONS_H_

#include <set>
#include <string>

#include "base/component_export.h"
#include "base/macros.h"

namespace catalog {

struct COMPONENT_EXPORT(CATALOG) ServiceOptions {
  enum class InstanceSharingType {
    NONE,
    SINGLETON,
    SHARED_ACROSS_INSTANCE_GROUPS,
  };

  ServiceOptions();
  ServiceOptions(const ServiceOptions& other);
  ~ServiceOptions();

  InstanceSharingType instance_sharing = InstanceSharingType::NONE;
  bool can_connect_to_instances_in_any_group = false;
  bool can_connect_to_other_services_with_any_instance_name = false;
  bool can_create_other_service_instances = false;
  std::set<std::string> interfaces_bindable_on_any_service;
};

}  // namespace catalog

#endif  // SERVICES_CATALOG_SERVICE_OPTIONS_H_
