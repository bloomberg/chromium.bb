// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_COORDINATION_UNIT_BASE_H_
#define SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_COORDINATION_UNIT_BASE_H_

#include <memory>

#include "base/callback.h"
#include "base/observer_list.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_graph.h"
#include "services/resource_coordinator/observers/coordination_unit_graph_observer.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_types.h"
#include "services/resource_coordinator/public/mojom/coordination_unit.mojom.h"
#include "services/resource_coordinator/public/mojom/coordination_unit_provider.mojom.h"
#include "services/service_manager/public/cpp/service_context_ref.h"

namespace resource_coordinator {

class CoordinationUnitGraph;

// CoordinationUnitBase implements shared functionality among different types of
// coordination units. A specific type of coordination unit will derive from
// this class and can override shared funtionality when needed.
class CoordinationUnitBase {
 public:
  CoordinationUnitBase(const CoordinationUnitID& id,
                       CoordinationUnitGraph* graph);
  virtual ~CoordinationUnitBase();

  void Destruct();
  void BeforeDestroyed();
  void AddObserver(CoordinationUnitGraphObserver* observer);
  void RemoveObserver(CoordinationUnitGraphObserver* observer);
  bool GetProperty(const mojom::PropertyType property_type,
                   int64_t* result) const;
  int64_t GetPropertyOrDefault(const mojom::PropertyType property_type,
                               int64_t default_value) const;

  const CoordinationUnitID& id() const { return id_; }
  CoordinationUnitGraph* graph() const { return graph_; }

  const base::ObserverList<CoordinationUnitGraphObserver>& observers() const {
    return observers_;
  }

  void SetPropertyForTesting(int64_t value) {
    SetProperty(mojom::PropertyType::kTest, value);
  }

  const std::map<mojom::PropertyType, int64_t>& properties_for_testing() const {
    return properties_;
  }

 protected:
  virtual void OnEventReceived(mojom::Event event);
  virtual void OnPropertyChanged(mojom::PropertyType property_type,
                                 int64_t value);

  void SendEvent(mojom::Event event);
  void SetProperty(mojom::PropertyType property_type, int64_t value);

  // Passes the ownership of the newly created |new_cu| to its graph.
  static CoordinationUnitBase* PassOwnershipToGraph(
      std::unique_ptr<CoordinationUnitBase> new_cu);

  CoordinationUnitGraph* const graph_;
  const CoordinationUnitID id_;

 private:
  base::ObserverList<CoordinationUnitGraphObserver> observers_;
  std::map<mojom::PropertyType, int64_t> properties_;

  DISALLOW_COPY_AND_ASSIGN(CoordinationUnitBase);
};

template <class CoordinationUnitClass,
          class MojoInterfaceClass,
          class MojoRequestClass>
class CoordinationUnitInterface : public CoordinationUnitBase,
                                  public MojoInterfaceClass {
 public:
  static CoordinationUnitClass* Create(
      const CoordinationUnitID& id,
      CoordinationUnitGraph* graph,
      std::unique_ptr<service_manager::ServiceContextRef> service_ref) {
    std::unique_ptr<CoordinationUnitClass> new_cu =
        std::make_unique<CoordinationUnitClass>(id, graph,
                                                std::move(service_ref));
    return static_cast<CoordinationUnitClass*>(
        PassOwnershipToGraph(std::move(new_cu)));
  }

  static const CoordinationUnitClass* FromCoordinationUnitBase(
      const CoordinationUnitBase* cu) {
    DCHECK(cu->id().type == CoordinationUnitClass::Type());
    return static_cast<const CoordinationUnitClass*>(cu);
  }

  static CoordinationUnitClass* FromCoordinationUnitBase(
      CoordinationUnitBase* cu) {
    DCHECK(cu->id().type == CoordinationUnitClass::Type());
    return static_cast<CoordinationUnitClass*>(cu);
  }

  CoordinationUnitInterface(
      const CoordinationUnitID& id,
      CoordinationUnitGraph* graph,

      std::unique_ptr<service_manager::ServiceContextRef> service_ref)
      : CoordinationUnitBase(id, graph), binding_(this) {
    service_ref_ = std::move(service_ref);
  }

  ~CoordinationUnitInterface() override = default;

  void Bind(MojoRequestClass request) { binding_.Bind(std::move(request)); }

  void GetID(typename MojoInterfaceClass::GetIDCallback callback) override {
    std::move(callback).Run(id_);
  }
  void AddBinding(MojoRequestClass request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  mojo::Binding<MojoInterfaceClass>& binding() { return binding_; }

 protected:
  static CoordinationUnitClass* GetCoordinationUnitByID(
      CoordinationUnitGraph* graph,
      const CoordinationUnitID cu_id) {
    DCHECK(cu_id.type == CoordinationUnitClass::Type());
    auto* cu = graph->GetCoordinationUnitByID(cu_id);
    DCHECK(cu->id().type == CoordinationUnitClass::Type());
    return static_cast<CoordinationUnitClass*>(cu);
  }

 private:
  mojo::BindingSet<MojoInterfaceClass> bindings_;
  mojo::Binding<MojoInterfaceClass> binding_;
  std::unique_ptr<service_manager::ServiceContextRef> service_ref_;

  DISALLOW_COPY_AND_ASSIGN(CoordinationUnitInterface);
};

}  // namespace resource_coordinator

#endif  // SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_COORDINATION_UNIT_BASE_H_
