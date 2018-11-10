// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_IDENTITY_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_IDENTITY_H_

#include <string>

#include "base/optional.h"
#include "base/token.h"
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
// |instance_group| is a base::Token representing the identity of an isolated
// group of instances running in the system. Most services may only connect to
// instances in the same instance group as themselves. This group is implied
// when |instance_group| is null.
//
// |instance_id| identifies a more specific instance within the targeted
// instance group, or globally if the target's instances are shared across
// instance groups. Typically this is also null unless a client knows there are
// multiple instances of the target service and wants to connect to a specific
// one.
//
// Finally, every service instance also has a globally unique ID Token
// associated with it. This Token is generated and assigned by the Service
// Manager on instance startup.
//
// If an interface request targets an Identity with a specific globally unique
// ID, that request will be processed only if the instance identified by that ID
// is still running.
//
// TODO(https://crbug.com/902590): Also consider whether partial Identity
// values used for service instance resolution (i.e. passed to
// |Connector.BindInterface()|) should be their own kind of type.
class SERVICE_MANAGER_PUBLIC_CPP_TYPES_EXPORT Identity {
 public:
  Identity();
  explicit Identity(const std::string& name);
  Identity(const std::string& name,
           const base::Optional<base::Token>& instance_group);
  Identity(const std::string& name,
           const base::Optional<base::Token>& instance_group,
           const base::Optional<base::Token>& instance_id);
  Identity(const std::string& name,
           const base::Optional<base::Token>& instance_group,
           const base::Optional<base::Token>& instance_id,
           const base::Optional<base::Token>& globally_unique_id);
  Identity(const Identity& other);
  ~Identity();

  Identity& operator=(const Identity& other);
  bool operator<(const Identity& other) const;
  bool operator==(const Identity& other) const;
  bool operator!=(const Identity& other) const { return !(*this == other); }

  bool IsValid() const;

  std::string ToString() const;

  // Indicates whether this Identity matches |other|, meaning that the name,
  // instance group, and instance ID are identical in both. Note in particular
  // that GUID is completely ignored, so this is not a strict equality test.
  // For strict equality including GUID, use |operator==|.
  bool Matches(const Identity& other) const;

  // Returns a copy of this Identity with no globally unique ID specified.
  Identity WithoutGloballyUniqueId() const;

  const std::string& name() const { return name_; }
  const base::Optional<base::Token>& instance_group() const {
    return instance_group_;
  }
  void set_instance_group(const base::Optional<base::Token>& instance_group) {
    instance_group_ = instance_group;
  }
  const base::Optional<base::Token>& instance_id() const {
    return instance_id_;
  }
  void set_instance_id(const base::Optional<base::Token>& instance_id) {
    instance_id_ = instance_id;
  }
  const base::Optional<base::Token>& globally_unique_id() const {
    return globally_unique_id_;
  }

 private:
  std::string name_;
  base::Optional<base::Token> instance_group_;
  base::Optional<base::Token> instance_id_;
  base::Optional<base::Token> globally_unique_id_;
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_IDENTITY_H_
