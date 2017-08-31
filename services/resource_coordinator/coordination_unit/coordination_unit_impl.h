// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_COORDINATION_UNIT_IMPL_H_
#define SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_COORDINATION_UNIT_IMPL_H_

#include <memory>
#include <set>

#include "base/callback.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_types.h"
#include "services/resource_coordinator/public/interfaces/coordination_unit.mojom.h"
#include "services/resource_coordinator/public/interfaces/coordination_unit_provider.mojom.h"
#include "services/service_manager/public/cpp/service_context_ref.h"

namespace resource_coordinator {

class CoordinationUnitGraphObserver;
class FrameCoordinationUnitImpl;
class WebContentsCoordinationUnitImpl;

class CoordinationUnitImpl : public mojom::CoordinationUnit {
 public:
  CoordinationUnitImpl(
      const CoordinationUnitID& id,
      std::unique_ptr<service_manager::ServiceContextRef> service_ref);

  ~CoordinationUnitImpl() override;

  static const FrameCoordinationUnitImpl* ToFrameCoordinationUnit(
      const CoordinationUnitImpl* coordination_unit);
  static WebContentsCoordinationUnitImpl* ToWebContentsCoordinationUnit(
      CoordinationUnitImpl* coordination_unit);
  static const WebContentsCoordinationUnitImpl* ToWebContentsCoordinationUnit(
      const CoordinationUnitImpl* coordination_unit);

  static std::vector<CoordinationUnitImpl*> GetCoordinationUnitsOfType(
      CoordinationUnitType type);

  static CoordinationUnitImpl* CreateCoordinationUnit(
      const CoordinationUnitID& id,
      std::unique_ptr<service_manager::ServiceContextRef> service_ref);

  static void AssertNoActiveCoordinationUnits();
  static void ClearAllCoordinationUnits();

  void Destruct();
  void Bind(mojom::CoordinationUnitRequest request);

  // Overridden from mojom::CoordinationUnit:
  void GetID(const GetIDCallback& callback) override;
  void AddBinding(mojom::CoordinationUnitRequest request) override;
  void AddChild(const CoordinationUnitID& child_id) override;
  void RemoveChild(const CoordinationUnitID& child_id) override;
  void SendEvent(mojom::Event event) override;
  void SetProperty(mojom::PropertyType property_type, int64_t value) override;

  // Return all of the reachable |CoordinationUnitImpl| instances
  // of type |CoordinationUnitType|. Note that a callee should
  // never be associated with itself.
  virtual std::set<CoordinationUnitImpl*> GetAssociatedCoordinationUnitsOfType(
      CoordinationUnitType type) const;
  // Recalculate property internally.
  virtual void RecalculateProperty(const mojom::PropertyType property_type) {}

  // Operations performed on the internal key-value store.
  bool GetProperty(const mojom::PropertyType property_type,
                   int64_t* result) const;

  // Methods utilized by the |CoordinationUnitGraphObserver| framework.
  void BeforeDestroyed();
  void AddObserver(CoordinationUnitGraphObserver* observer);
  void RemoveObserver(CoordinationUnitGraphObserver* observer);

  // Getters and setters.
  const CoordinationUnitID& id() const { return id_; }
  const std::set<CoordinationUnitImpl*>& children() const { return children_; }
  const std::set<CoordinationUnitImpl*>& parents() const { return parents_; }
  const std::map<mojom::PropertyType, int64_t>& properties_for_testing() const {
    return properties_;
  }
  mojo::Binding<mojom::CoordinationUnit>& binding() { return binding_; }

 protected:
  friend class FrameCoordinationUnitImpl;

  virtual void OnEventReceived(const mojom::Event event);
  virtual void OnPropertyChanged(const mojom::PropertyType property_type,
                                 int64_t value);
  // Propagate property change to relevant |CoordinationUnitImpl| instances.
  virtual void PropagateProperty(mojom::PropertyType property_type,
                                 int64_t value) {}

  // Coordination unit graph traversal helper functions.
  std::set<CoordinationUnitImpl*> GetChildCoordinationUnitsOfType(
      CoordinationUnitType type) const;
  std::set<CoordinationUnitImpl*> GetParentCoordinationUnitsOfType(
      CoordinationUnitType type) const;

  const base::ObserverList<CoordinationUnitGraphObserver>& observers() const {
    return observers_;
  }

  const CoordinationUnitID id_;
  std::set<CoordinationUnitImpl*> children_;
  std::set<CoordinationUnitImpl*> parents_;

 private:
  bool AddChild(CoordinationUnitImpl* child);
  bool RemoveChild(CoordinationUnitImpl* child);
  void AddParent(CoordinationUnitImpl* parent);
  void RemoveParent(CoordinationUnitImpl* parent);
  bool HasAncestor(CoordinationUnitImpl* ancestor);
  bool HasDescendant(CoordinationUnitImpl* descendant);

  std::map<mojom::PropertyType, int64_t> properties_;

  std::unique_ptr<service_manager::ServiceContextRef> service_ref_;
  mojo::BindingSet<mojom::CoordinationUnit> bindings_;

  base::ObserverList<CoordinationUnitGraphObserver> observers_;
  mojo::Binding<mojom::CoordinationUnit> binding_;

  DISALLOW_COPY_AND_ASSIGN(CoordinationUnitImpl);
};

}  // namespace resource_coordinator

#endif  // SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_COORDINATION_UNIT_IMPL_H_
