// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/coordination_unit_base.h"

#include <unordered_map>

#include "base/strings/string_number_conversions.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/resource_coordinator/coordination_unit/frame_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/page_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/process_coordination_unit_impl.h"
#include "services/resource_coordinator/observers/coordination_unit_graph_observer.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_id.h"

namespace resource_coordinator {

namespace {

using CUIDMap = std::unordered_map<CoordinationUnitID,
                                   std::unique_ptr<CoordinationUnitBase>>;

CUIDMap& g_cu_map() {
  static CUIDMap* instance = new CUIDMap();
  return *instance;
}

}  // namespace

// static
const FrameCoordinationUnitImpl* CoordinationUnitBase::ToFrameCoordinationUnit(
    const CoordinationUnitBase* coordination_unit) {
  DCHECK(coordination_unit->id().type == CoordinationUnitType::kFrame);
  return static_cast<const FrameCoordinationUnitImpl*>(coordination_unit);
}

// static
PageCoordinationUnitImpl* CoordinationUnitBase::ToPageCoordinationUnit(
    CoordinationUnitBase* coordination_unit) {
  DCHECK(coordination_unit->id().type == CoordinationUnitType::kPage);
  return static_cast<PageCoordinationUnitImpl*>(coordination_unit);
}

// static
const PageCoordinationUnitImpl* CoordinationUnitBase::ToPageCoordinationUnit(
    const CoordinationUnitBase* cu) {
  DCHECK(cu->id().type == CoordinationUnitType::kPage);
  return static_cast<const PageCoordinationUnitImpl*>(cu);
}

// static
std::vector<CoordinationUnitBase*>
CoordinationUnitBase::GetCoordinationUnitsOfType(CoordinationUnitType type) {
  std::vector<CoordinationUnitBase*> results;
  for (auto& el : g_cu_map()) {
    if (el.second->id().type == type)
      results.push_back(el.second.get());
  }
  return results;
}

void CoordinationUnitBase::AssertNoActiveCoordinationUnits() {
  CHECK(g_cu_map().empty());
}

void CoordinationUnitBase::ClearAllCoordinationUnits() {
  g_cu_map().clear();
}

CoordinationUnitBase* CoordinationUnitBase::CreateCoordinationUnit(
    const CoordinationUnitID& id,
    std::unique_ptr<service_manager::ServiceContextRef> service_ref) {
  std::unique_ptr<CoordinationUnitBase> new_cu;

  switch (id.type) {
    case CoordinationUnitType::kFrame:
      new_cu = base::MakeUnique<FrameCoordinationUnitImpl>(
          id, std::move(service_ref));
      break;
    case CoordinationUnitType::kProcess:
      new_cu = base::MakeUnique<ProcessCoordinationUnitImpl>(
          id, std::move(service_ref));
      break;
    case CoordinationUnitType::kPage:
      new_cu = base::MakeUnique<PageCoordinationUnitImpl>(
          id, std::move(service_ref));
      break;
    default:
      NOTREACHED();
  }

  auto it = g_cu_map().insert(std::make_pair(new_cu->id(), std::move(new_cu)));
  DCHECK(it.second);  // Inserted successfully
  return it.first->second.get();
}

void CoordinationUnitBase::Destruct() {
  size_t erased_count = g_cu_map().erase(id_);
  // After this point |this| is destructed and should not be accessed anymore.
  DCHECK_EQ(erased_count, 1u);
}

CoordinationUnitBase::CoordinationUnitBase(
    const CoordinationUnitID& id,
    std::unique_ptr<service_manager::ServiceContextRef> service_ref)
    : id_(id.type, id.id), binding_(this) {
  service_ref_ = std::move(service_ref);
}

CoordinationUnitBase::~CoordinationUnitBase() {
  for (CoordinationUnitBase* child : children_) {
    child->RemoveParent(this);
  }

  for (CoordinationUnitBase* parent : parents_) {
    parent->RemoveChild(this);
  }
}

void CoordinationUnitBase::Bind(mojom::CoordinationUnitRequest request) {
  binding_.Bind(std::move(request));
}

void CoordinationUnitBase::SendEvent(mojom::Event event) {
  OnEventReceived(event);
}

void CoordinationUnitBase::GetID(const GetIDCallback& callback) {
  callback.Run(id_);
}

void CoordinationUnitBase::AddBinding(mojom::CoordinationUnitRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void CoordinationUnitBase::AddChild(const CoordinationUnitID& child_id) {
  // TODO(ojan): Make this a DCHECK. When does it make sense to add a
  // CoordinationUnit as its own child?
  if (child_id == id_) {
    return;
  }

  auto child_iter = g_cu_map().find(child_id);
  if (child_iter != g_cu_map().end()) {
    CoordinationUnitBase* child = child_iter->second.get();
    // In order to avoid cyclic reference inside the coordination unit graph. If
    // |child| is one of the ancestors of |this| coordination unit, then |child|
    // should not be added, abort this operation. If |this| coordination unit is
    // one of the descendants of child coordination unit, then abort this
    // operation.
    if (HasAncestor(child) || child->HasDescendant(this)) {
      DCHECK(false) << "Cyclic reference in coordination unit graph detected!";
      return;
    }

    DCHECK(child->id_ == child_id);
    DCHECK(child != this);

    if (AddChild(child)) {
      child->AddParent(this);
    }
  }
}

bool CoordinationUnitBase::AddChild(CoordinationUnitBase* child) {
  bool success =
      children_.count(child) ? false : children_.insert(child).second;
  return success;
}

void CoordinationUnitBase::RemoveChild(const CoordinationUnitID& child_id) {
  auto child_iter = g_cu_map().find(child_id);
  if (child_iter == g_cu_map().end()) {
    return;
  }

  CoordinationUnitBase* child = child_iter->second.get();

  DCHECK(child->id_ == child_id);
  DCHECK(child != this);

  if (RemoveChild(child)) {
    child->RemoveParent(this);
  }
}

bool CoordinationUnitBase::RemoveChild(CoordinationUnitBase* child) {
  size_t children_removed = children_.erase(child);
  return children_removed > 0;
}

void CoordinationUnitBase::AddParent(CoordinationUnitBase* parent) {
  DCHECK_EQ(0u, parents_.count(parent));
  parents_.insert(parent);
}

void CoordinationUnitBase::RemoveParent(CoordinationUnitBase* parent) {
  size_t parents_removed = parents_.erase(parent);
  DCHECK_EQ(1u, parents_removed);
}

bool CoordinationUnitBase::HasAncestor(CoordinationUnitBase* ancestor) {
  for (CoordinationUnitBase* parent : parents_) {
    if (parent == ancestor || parent->HasAncestor(ancestor)) {
      return true;
    }
  }

  return false;
}

bool CoordinationUnitBase::HasDescendant(CoordinationUnitBase* descendant) {
  for (CoordinationUnitBase* child : children_) {
    if (child == descendant || child->HasDescendant(descendant)) {
      return true;
    }
  }

  return false;
}

std::set<CoordinationUnitBase*>
CoordinationUnitBase::GetChildCoordinationUnitsOfType(
    CoordinationUnitType type) const {
  std::set<CoordinationUnitBase*> coordination_units;

  for (auto* child : children()) {
    if (child->id().type != type)
      continue;
    coordination_units.insert(child);
    for (auto* coordination_unit :
         child->GetChildCoordinationUnitsOfType(type)) {
      coordination_units.insert(coordination_unit);
    }
  }

  return coordination_units;
}

std::set<CoordinationUnitBase*>
CoordinationUnitBase::GetParentCoordinationUnitsOfType(
    CoordinationUnitType type) const {
  std::set<CoordinationUnitBase*> coordination_units;

  for (auto* parent : parents()) {
    if (parent->id().type != type)
      continue;
    coordination_units.insert(parent);
    for (auto* coordination_unit :
         parent->GetParentCoordinationUnitsOfType(type)) {
      coordination_units.insert(coordination_unit);
    }
  }

  return coordination_units;
}

bool CoordinationUnitBase::GetProperty(const mojom::PropertyType property_type,
                                       int64_t* result) const {
  auto value_it = properties_.find(property_type);

  if (value_it != properties_.end()) {
    *result = value_it->second;
    return true;
  }

  return false;
}

void CoordinationUnitBase::SetProperty(mojom::PropertyType property_type,
                                       int64_t value) {
  // The |CoordinationUnitGraphObserver| API specification dictates that
  // the property is guarranteed to be set on the |CoordinationUnitBase|
  // and propagated to the appropriate associated |CoordianationUnitBase|
  // before |OnPropertyChanged| is invoked on all of the registered observers.
  properties_[property_type] = value;
  PropagateProperty(property_type, value);
  OnPropertyChanged(property_type, value);
}

void CoordinationUnitBase::BeforeDestroyed() {
  for (auto& observer : observers_)
    observer.OnBeforeCoordinationUnitDestroyed(this);
}

void CoordinationUnitBase::AddObserver(
    CoordinationUnitGraphObserver* observer) {
  observers_.AddObserver(observer);
}

void CoordinationUnitBase::RemoveObserver(
    CoordinationUnitGraphObserver* observer) {
  observers_.RemoveObserver(observer);
}

void CoordinationUnitBase::OnEventReceived(const mojom::Event event) {
  for (auto& observer : observers_)
    observer.OnEventReceived(this, event);
}

void CoordinationUnitBase::OnPropertyChanged(
    const mojom::PropertyType property_type,
    int64_t value) {
  for (auto& observer : observers_)
    observer.OnPropertyChanged(this, property_type, value);
}

}  // namespace resource_coordinator
