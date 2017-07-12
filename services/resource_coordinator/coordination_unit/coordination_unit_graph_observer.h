// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_COORDINATION_UNIT_GRAPH_OBSERVER_H_
#define SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_COORDINATION_UNIT_GRAPH_OBSERVER_H_

#include "base/macros.h"
#include "services/resource_coordinator/public/interfaces/coordination_unit.mojom.h"

namespace resource_coordinator {

class CoordinationUnitImpl;

// An observer API for the coordination unit graph maintained by GRC.
//
// Observers are instantiated when the resource_coordinator service
// is created and are destroyed when the resource_coordinator service
// is destroyed. Therefore observers are guaranteed to be alive before
// any coordination unit is created and will be alive after any
// coordination unit is destroyed. Additionally, any
// Coordination Unit reachable within a callback will always be
// initialized and valid.
//
// To create and install a new observer:
//   (1) Derive from this class.
//   (2) Register by calling on |coordination_unit_manager().ResgiterObserver|
//       inside of the ResourceCoordinatorService::Create.
class CoordinationUnitGraphObserver {
 public:
  CoordinationUnitGraphObserver();
  virtual ~CoordinationUnitGraphObserver();

  // Determines whether or not the observer should be registered with, and
  // invoked for, the |coordination_unit|.
  virtual bool ShouldObserve(const CoordinationUnitImpl* coordination_unit) = 0;

  // Called whenever a CoordinationUnit is created.
  virtual void OnCoordinationUnitCreated(
      const CoordinationUnitImpl* coordination_unit) {}

  // Called whenever a new parent-child relationship occurs where the
  // |coordination_unit| is the parent of |child_coordination_unit|
  virtual void OnChildAdded(
      const CoordinationUnitImpl* coordination_unit,
      const CoordinationUnitImpl* child_coordination_unit) {}

  // Called whenever a new parent-child relationship occurs where the
  // |coordination_unit| is the child of |parent_coordination_unit|.
  virtual void OnParentAdded(
      const CoordinationUnitImpl* coordination_unit,
      const CoordinationUnitImpl* parent_coordination_unit) {}

  // Called whenever a |property| within the |coordination_unit|'s
  // internal property store has changed.
  virtual void OnPropertyChanged(const CoordinationUnitImpl* coordination_unit,
                                 const mojom::PropertyType property_type,
                                 const base::Value& value) {}

  // Called whenever parent-child relationship ends where the
  // |coordination_unit| was the parent and the |child_coordination_unit|.
  virtual void OnChildRemoved(
      const CoordinationUnitImpl* coordination_unit,
      const CoordinationUnitImpl* child_coordination_unit) {}

  // Called whenever parent-child relationship ends where the
  // |coordination_unit| was the child and the |child_coordination_unit|.
  virtual void OnParentRemoved(
      const CoordinationUnitImpl* coordination_unit,
      const CoordinationUnitImpl* parent_coordination_unit) {}

  // Called when the |coordination_unit| is about to be destroyed.
  virtual void OnBeforeCoordinationUnitDestroyed(
      const CoordinationUnitImpl* coordination_unit) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CoordinationUnitGraphObserver);
};

}  // namespace resource_coordinator

#endif  // SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_COORDINATION_UNIT_GRAPH_OBSERVER_H_
