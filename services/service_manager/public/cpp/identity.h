// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_IDENTITY_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_IDENTITY_H_

#include <string>

#include "services/service_manager/public/cpp/types_export.h"

namespace service_manager {

// Represents the identity of a service instance. Useful to clients who want to
// connect to running (or lazily started) service instances through the Service
// Manager. Every Service also has a unique Identity assigned to it by the
// Service Manager on service startup, communicated to the service instance via
// |Service::OnStart()|.
//
// |name| is the name of the service, as specified in the service's manifest.
//
// |instance_group| is a GUID string representing the identity of an isolated
// group of instances running in the system. Most services may only connect to
// instances in the same instance group as themselves. This group is implied
// when |instance_group| is empty.
//
// |instance_id| identifies a more specific instance within the targeted
// instance group, or globally if the target's instances are shared across
// instance groups. Typically this is also the empty string unless a client
// knows there are multiple instances of the target service and wants to connect
// to a specific one.
//
// TODO(https://crbug.com/895591): Switch |instance_group| and |instance_id|
// fields to base::Optional<base::UnguessableToken> instead of free-form
// strings.
class SERVICE_MANAGER_PUBLIC_CPP_TYPES_EXPORT Identity {
 public:
  Identity();
  explicit Identity(const std::string& name);
  Identity(const std::string& name, const std::string& instance_group);
  Identity(const std::string& name,
           const std::string& instance_group,
           const std::string& instance_id);
  Identity(const Identity& other);
  ~Identity();

  Identity& operator=(const Identity& other);
  bool operator<(const Identity& other) const;
  bool operator==(const Identity& other) const;

  bool IsValid() const;

  const std::string& name() const { return name_; }
  const std::string& instance_group() const { return instance_group_; }
  void set_instance_group(const std::string& instance_group) {
    instance_group_ = instance_group;
  }
  const std::string& instance_id() const { return instance_id_; }

 private:
  std::string name_;
  std::string instance_group_;
  std::string instance_id_;
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_IDENTITY_H_
