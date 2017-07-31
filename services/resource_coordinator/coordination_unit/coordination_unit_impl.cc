// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/coordination_unit_impl.h"

#include <unordered_map>

#include "base/strings/string_number_conversions.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_graph_observer.h"
#include "services/resource_coordinator/coordination_unit/frame_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/process_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/web_contents_coordination_unit_impl.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_id.h"

namespace resource_coordinator {

#define NOTIFY_OBSERVERS(observers, Method, ...) \
  for (auto& observer : observers) {             \
    observer.Method(__VA_ARGS__);                \
  }

namespace {

using CUIDMap = std::unordered_map<CoordinationUnitID,
                                   std::unique_ptr<CoordinationUnitImpl>>;

CUIDMap& g_cu_map() {
  static CUIDMap* instance = new CUIDMap();
  return *instance;
}

}  // namespace

// static
const FrameCoordinationUnitImpl* CoordinationUnitImpl::ToFrameCoordinationUnit(
    const CoordinationUnitImpl* coordination_unit) {
  DCHECK(coordination_unit->id().type == CoordinationUnitType::kFrame);
  return static_cast<const FrameCoordinationUnitImpl*>(coordination_unit);
}

// static
std::vector<CoordinationUnitImpl*>
CoordinationUnitImpl::GetCoordinationUnitsOfType(CoordinationUnitType type) {
  std::vector<CoordinationUnitImpl*> results;
  for (auto& el : g_cu_map()) {
    if (el.second->id().type == type)
      results.push_back(el.second.get());
  }
  return results;
}

void CoordinationUnitImpl::AssertNoActiveCoordinationUnits() {
  CHECK(g_cu_map().empty());
}

void CoordinationUnitImpl::ClearAllCoordinationUnits() {
  g_cu_map().clear();
}

CoordinationUnitImpl* CoordinationUnitImpl::CreateCoordinationUnit(
    const CoordinationUnitID& id,
    std::unique_ptr<service_manager::ServiceContextRef> service_ref) {
  std::unique_ptr<CoordinationUnitImpl> new_cu;

  switch (id.type) {
    case CoordinationUnitType::kFrame:
      new_cu = base::MakeUnique<FrameCoordinationUnitImpl>(
          id, std::move(service_ref));
      break;
    case CoordinationUnitType::kProcess:
      new_cu = base::MakeUnique<ProcessCoordinationUnitImpl>(
          id, std::move(service_ref));
      break;
    case CoordinationUnitType::kWebContents:
      new_cu = base::MakeUnique<WebContentsCoordinationUnitImpl>(
          id, std::move(service_ref));
      break;
    default:
      new_cu =
          base::MakeUnique<CoordinationUnitImpl>(id, std::move(service_ref));
  }
  CoordinationUnitImpl* new_cu_ptr = new_cu.get();
  auto it = g_cu_map().insert(std::make_pair(new_cu->id(), std::move(new_cu)));
  DCHECK(it.second);  // Inserted successfully

  return new_cu_ptr;
}

void CoordinationUnitImpl::Destruct() {
  size_t erased_count = g_cu_map().erase(id_);
  // After this point |this| is destructed and should not be accessed anymore.
  DCHECK_EQ(erased_count, 1u);
}

CoordinationUnitImpl::CoordinationUnitImpl(
    const CoordinationUnitID& id,
    std::unique_ptr<service_manager::ServiceContextRef> service_ref)
    : id_(id.type, id.id), binding_(this) {
  service_ref_ = std::move(service_ref);
}

CoordinationUnitImpl::~CoordinationUnitImpl() {
  for (CoordinationUnitImpl* child : children_) {
    child->RemoveParent(this);
  }

  for (CoordinationUnitImpl* parent : parents_) {
    parent->RemoveChild(this);
  }
}

void CoordinationUnitImpl::Bind(mojom::CoordinationUnitRequest request) {
  binding_.Bind(std::move(request));
}

bool CoordinationUnitImpl::SelfOrParentHasFlagSet(StateFlags state) {
  const base::Optional<bool>& state_flag = state_flags_[state];
  if (state_flag && *state_flag) {
    return true;
  }

  for (CoordinationUnitImpl* parent : parents_) {
    if (parent->SelfOrParentHasFlagSet(state)) {
      return true;
    }
  }

  return false;
}

void CoordinationUnitImpl::RecalcCoordinationPolicy() {
  for (CoordinationUnitImpl* child : children_) {
    child->RecalcCoordinationPolicy();
  }

  if (!policy_callback_) {
    return;
  }

  bool background_priority = !SelfOrParentHasFlagSet(kTabVisible) &&
                             !SelfOrParentHasFlagSet(kAudioPlaying);

  // Send the priority to the client if it's new or changed.
  if (!current_policy_) {
    current_policy_ = mojom::CoordinationPolicy::New();
  } else if ((current_policy_->use_background_priority ==
              background_priority) &&
             !SelfOrParentHasFlagSet(StateFlags::kTestState)) {
    return;
  }

  // current_policy_ should be kept in sync with the policy we
  // send to the client, to avoid redundant updates.
  // TODO(oysteine): Once this object becomes more complex, make
  // copying more robust.
  mojom::CoordinationPolicyPtr policy = mojom::CoordinationPolicy::New();
  policy->use_background_priority = background_priority;
  current_policy_->use_background_priority = background_priority;

  policy_callback_->SetCoordinationPolicy(std::move(policy));
}

void CoordinationUnitImpl::SendEvent(mojom::EventPtr event) {
  // TODO(crbug.com/691886) Consider removing the following code.
  switch (event->type) {
    case mojom::EventType::kOnWebContentsShown:
      state_flags_[kTabVisible] = true;
      break;
    case mojom::EventType::kOnWebContentsHidden:
      state_flags_[kTabVisible] = false;
      break;
    case mojom::EventType::kOnProcessAudioStarted:
      state_flags_[kAudioPlaying] = true;
      break;
    case mojom::EventType::kOnProcessAudioStopped:
      state_flags_[kAudioPlaying] = false;
      break;
    case mojom::EventType::kTestEvent:
      state_flags_[kTestState] = true;
      break;
    default:
      return;
  }

  RecalcCoordinationPolicy();
}

void CoordinationUnitImpl::GetID(const GetIDCallback& callback) {
  callback.Run(id_);
}

void CoordinationUnitImpl::AddBinding(mojom::CoordinationUnitRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void CoordinationUnitImpl::AddChild(const CoordinationUnitID& child_id) {
  if (child_id == id_) {
    return;
  }

  auto child_iter = g_cu_map().find(child_id);
  if (child_iter != g_cu_map().end()) {
    CoordinationUnitImpl* child = child_iter->second.get();
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

bool CoordinationUnitImpl::AddChild(CoordinationUnitImpl* child) {
  // We don't recalculate the policy here as policies are only dependent
  // on the current CU or its parents, not its children. In other words,
  // policies only bubble down.
  bool success =
      children_.count(child) ? false : children_.insert(child).second;

  if (success) {
    NOTIFY_OBSERVERS(observers_, OnChildAdded, this, child);
  }

  return success;
}

void CoordinationUnitImpl::RemoveChild(const CoordinationUnitID& child_id) {
  auto child_iter = g_cu_map().find(child_id);
  if (child_iter == g_cu_map().end()) {
    return;
  }

  CoordinationUnitImpl* child = child_iter->second.get();

  DCHECK(child->id_ == child_id);
  DCHECK(child != this);

  if (RemoveChild(child)) {
    child->RemoveParent(this);
  }
}

bool CoordinationUnitImpl::RemoveChild(CoordinationUnitImpl* child) {
  size_t children_removed = children_.erase(child);
  bool success = children_removed > 0;

  if (success) {
    NOTIFY_OBSERVERS(observers_, OnChildRemoved, this, child);
  }

  return success;
}

void CoordinationUnitImpl::AddParent(CoordinationUnitImpl* parent) {
  DCHECK_EQ(0u, parents_.count(parent));
  parents_.insert(parent);

  NOTIFY_OBSERVERS(observers_, OnParentAdded, this, parent);

  RecalcCoordinationPolicy();
}

void CoordinationUnitImpl::RemoveParent(CoordinationUnitImpl* parent) {
  size_t parents_removed = parents_.erase(parent);
  DCHECK_EQ(1u, parents_removed);

  // TODO(matthalp, oysteine) should this go before or
  // after RecalcCoordinationPolicy?
  NOTIFY_OBSERVERS(observers_, OnParentRemoved, this, parent);

  RecalcCoordinationPolicy();
}

bool CoordinationUnitImpl::HasAncestor(CoordinationUnitImpl* ancestor) {
  for (CoordinationUnitImpl* parent : parents_) {
    if (parent == ancestor || parent->HasAncestor(ancestor)) {
      return true;
    }
  }

  return false;
}

bool CoordinationUnitImpl::HasDescendant(CoordinationUnitImpl* descendant) {
  for (CoordinationUnitImpl* child : children_) {
    if (child == descendant || child->HasDescendant(descendant)) {
      return true;
    }
  }

  return false;
}

void CoordinationUnitImpl::SetCoordinationPolicyCallback(
    mojom::CoordinationPolicyCallbackPtr callback) {
  callback.set_connection_error_handler(
      base::Bind(&CoordinationUnitImpl::UnregisterCoordinationPolicyCallback,
                 base::Unretained(this)));

  policy_callback_ = std::move(callback);

  RecalcCoordinationPolicy();
}

void CoordinationUnitImpl::UnregisterCoordinationPolicyCallback() {
  policy_callback_.reset();
  current_policy_.reset();
}

std::set<CoordinationUnitImpl*>
CoordinationUnitImpl::GetChildCoordinationUnitsOfType(
    CoordinationUnitType type) {
  std::set<CoordinationUnitImpl*> coordination_units;

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

std::set<CoordinationUnitImpl*>
CoordinationUnitImpl::GetParentCoordinationUnitsOfType(
    CoordinationUnitType type) {
  std::set<CoordinationUnitImpl*> coordination_units;

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

std::set<CoordinationUnitImpl*>
CoordinationUnitImpl::GetAssociatedCoordinationUnitsOfType(
    CoordinationUnitType type) {
  NOTREACHED();
  return std::set<CoordinationUnitImpl*>();
}

base::Value CoordinationUnitImpl::GetProperty(
    const mojom::PropertyType property_type) const {
  auto value_it = properties_.find(property_type);

  return value_it != properties_.end() ? base::Value(*value_it->second)
                                       : base::Value();
}

void CoordinationUnitImpl::SetProperty(mojom::PropertyType property_type,
                                       std::unique_ptr<base::Value> value) {
  // The |CoordinationUnitGraphObserver| API specification dictates that
  // the property is guarranteed to be set on the |CoordinationUnitImpl|
  // and propagated to the appropriate associated |CoordianationUnitImpl|
  // before |OnPropertyChanged| is invoked on all of the registered observers.
  const base::Value& property =
      *(properties_[property_type] = std::move(value));
  PropagateProperty(property_type, property);
  NOTIFY_OBSERVERS(observers_, OnPropertyChanged, this, property_type,
                   property);
}

void CoordinationUnitImpl::BeforeDestroyed() {
  NOTIFY_OBSERVERS(observers_, OnBeforeCoordinationUnitDestroyed, this);
}

void CoordinationUnitImpl::AddObserver(
    CoordinationUnitGraphObserver* observer) {
  observers_.AddObserver(observer);
}

void CoordinationUnitImpl::RemoveObserver(
    CoordinationUnitGraphObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace resource_coordinator
