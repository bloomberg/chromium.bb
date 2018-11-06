// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/identity.h"

#include "base/guid.h"
#include "services/service_manager/public/mojom/constants.mojom.h"

namespace service_manager {

Identity::Identity() : Identity("") {}

Identity::Identity(const std::string& name)
    : Identity(name, mojom::kInheritUserID) {}

Identity::Identity(const std::string& name, const std::string& instance_group)
    : Identity(name, instance_group, "") {}

Identity::Identity(const std::string& name,
                   const std::string& instance_group,
                   const std::string& instance_id)
    : name_(name), instance_group_(instance_group), instance_id_(instance_id) {
  DCHECK(!instance_group_.empty());
  DCHECK(base::IsValidGUID(instance_group_));
}

Identity::Identity(const Identity& other) = default;

Identity::~Identity() = default;

Identity& Identity::operator=(const Identity& other) = default;

bool Identity::operator<(const Identity& other) const {
  if (name_ != other.name_)
    return name_ < other.name_;
  if (instance_group_ != other.instance_group_)
    return instance_group_ < other.instance_group_;
  return instance_id_ < other.instance_id_;
}

bool Identity::operator==(const Identity& other) const {
  return name_ == other.name_ && instance_group_ == other.instance_group_ &&
         instance_id_ == other.instance_id_;
}

bool Identity::IsValid() const {
  return !name_.empty() && base::IsValidGUID(instance_group_);
}

}  // namespace service_manager
