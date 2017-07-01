// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_COORDINATION_UNIT_IMPL_H_
#define SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_COORDINATION_UNIT_IMPL_H_

#include <list>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_id.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_types.h"
#include "services/resource_coordinator/public/interfaces/coordination_unit.mojom.h"
#include "services/resource_coordinator/public/interfaces/coordination_unit_provider.mojom.h"
#include "services/service_manager/public/cpp/service_context_ref.h"

namespace resource_coordinator {

class CoordinationUnitGraphObserver;

class CoordinationUnitImpl : public mojom::CoordinationUnit {
 public:
  CoordinationUnitImpl(
      const CoordinationUnitID& id,
      std::unique_ptr<service_manager::ServiceContextRef> service_ref);
  ~CoordinationUnitImpl() override;

  // Utility functions for working with CoordinationUnitImpl instances.
  static bool IsCoordinationUnitType(
      const CoordinationUnitImpl* coordination_unit,
      CoordinationUnitType type);
  static bool IsFrameCoordinationUnit(
      const CoordinationUnitImpl* coordination_unit);
  static bool IsProcessCoordinationUnit(
      const CoordinationUnitImpl* coordination_unit);
  static bool IsWebContentsCoordinationUnit(
      const CoordinationUnitImpl* coordination_unit);

  // Return all of the reachable |CoordinationUnitImpl| instances
  // of type |CoordinationUnitType|. Note that a callee should
  // never be associated with itself.
  virtual std::set<CoordinationUnitImpl*> GetAssociatedCoordinationUnitsOfType(
      CoordinationUnitType type);

  // Recalculate property internally.
  virtual void RecalculateProperty(mojom::PropertyType property) {}
  // Propagate property change to relevant |CoordinationUnitImpl| instances
  // by invoking their their |RecalculateProperty|.
  virtual void PropagateProperty(mojom::PropertyType property) {}

  // Overridden from mojom::CoordinationUnit:
  void SendEvent(mojom::EventPtr event) override;
  void GetID(const GetIDCallback& callback) override;
  void AddBinding(mojom::CoordinationUnitRequest request) override;
  void AddChild(const CoordinationUnitID& child_id) override;
  void RemoveChild(const CoordinationUnitID& child_id) override;
  void SetCoordinationPolicyCallback(
      mojom::CoordinationPolicyCallbackPtr callback) override;
  void SetProperty(mojom::PropertyPtr property) override;

  // Operations performed on the internal key-value store.
  void ClearProperty(mojom::PropertyType property);
  base::Value GetProperty(mojom::PropertyType property) const;
  void SetProperty(mojom::PropertyType property, base::Value value);

  // Methods utilized by the |CoordinationUnitGraphObserver| framework.
  void WillBeDestroyed();
  void AddObserver(CoordinationUnitGraphObserver* observer);
  void RemoveObserver(CoordinationUnitGraphObserver* observer);

  // Getters and setters.
  const CoordinationUnitID& id() const { return id_; }
  const std::set<CoordinationUnitImpl*>& children() const { return children_; }
  const std::set<CoordinationUnitImpl*>& parents() const { return parents_; }
  const std::unordered_map<mojom::PropertyType, base::Value>&
  property_store_for_testing() const {
    return property_store_;
  }

 protected:
  const CoordinationUnitID id_;
  std::set<CoordinationUnitImpl*> children_;
  std::set<CoordinationUnitImpl*> parents_;

  // Coordination unit graph traversal helper functions.
  std::set<CoordinationUnitImpl*> GetChildCoordinationUnitsOfType(
      CoordinationUnitType type);
  std::set<CoordinationUnitImpl*> GetParentCoordinationUnitsOfType(
      CoordinationUnitType type);

 private:
  bool AddChild(CoordinationUnitImpl* child);
  bool RemoveChild(CoordinationUnitImpl* child);
  void AddParent(CoordinationUnitImpl* parent);
  void RemoveParent(CoordinationUnitImpl* parent);
  bool HasParent(CoordinationUnitImpl* unit);
  bool HasChild(CoordinationUnitImpl* unit);
  void RecalcCoordinationPolicy();
  void UnregisterCoordinationPolicyCallback();

  std::unordered_map<mojom::PropertyType, base::Value> property_store_;

  enum StateFlags : uint8_t {
    kTestState,
    kTabVisible,
    kAudioPlaying,
    kNetworkIdle,
    kNumStateFlags
  };
  bool SelfOrParentHasFlagSet(StateFlags state);

  std::unique_ptr<service_manager::ServiceContextRef> service_ref_;
  mojo::BindingSet<mojom::CoordinationUnit> bindings_;

  mojom::CoordinationPolicyCallbackPtr policy_callback_;
  mojom::CoordinationPolicyPtr current_policy_;

  base::ObserverList<CoordinationUnitGraphObserver> observers_;

  base::Optional<bool> state_flags_[kNumStateFlags];

  DISALLOW_COPY_AND_ASSIGN(CoordinationUnitImpl);
};

}  // namespace resource_coordinator

#endif  // SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_COORDINATION_UNIT_IMPL_H_
