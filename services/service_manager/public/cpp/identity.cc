// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/identity.h"

#include <tuple>

#include "base/strings/stringprintf.h"

namespace service_manager {

Identity::Identity() : Identity("") {}

Identity::Identity(const std::string& name) : Identity(name, base::nullopt) {}

Identity::Identity(const std::string& name,
                   const base::Optional<base::Token>& instance_group)
    : Identity(name, instance_group, "") {}

Identity::Identity(const std::string& name,
                   const base::Optional<base::Token>& instance_group,
                   const std::string& instance_id)
    : Identity(name, instance_group, instance_id, base::nullopt) {}

Identity::Identity(const std::string& name,
                   const base::Optional<base::Token>& instance_group,
                   const std::string& instance_id,
                   const base::Optional<base::Token>& globally_unique_id)
    : name_(name),
      instance_group_(instance_group),
      instance_id_(instance_id),
      globally_unique_id_(globally_unique_id) {
  DCHECK(!instance_group_ || !instance_group_->is_zero());
  DCHECK(!globally_unique_id_ || !globally_unique_id_->is_zero());
}

Identity::Identity(const Identity& other) = default;

Identity::~Identity() = default;

Identity& Identity::operator=(const Identity& other) = default;

bool Identity::operator<(const Identity& other) const {
  return std::tie(name_, instance_group_, instance_id_, globally_unique_id_) <
         std::tie(other.name_, other.instance_group_, other.instance_id_,
                  other.globally_unique_id_);
}

bool Identity::operator==(const Identity& other) const {
  return Matches(other) && globally_unique_id_ == other.globally_unique_id_;
}

bool Identity::IsValid() const {
  return !name_.empty();
}

std::string Identity::ToString() const {
  return base::StringPrintf(
      "%s/%s/%s/%s",
      instance_group_ ? instance_group_->ToString().c_str() : "*",
      name_.c_str(), instance_id_.c_str(),
      globally_unique_id_ ? globally_unique_id_->ToString().c_str() : "*");
}

bool Identity::Matches(const Identity& other) const {
  return name_ == other.name_ && instance_group_ == other.instance_group_ &&
         instance_id_ == other.instance_id_;
}

Identity Identity::WithoutGloballyUniqueId() const {
  return Identity(name_, instance_group_, instance_id_);
}

}  // namespace service_manager
