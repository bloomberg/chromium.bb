// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_GRAPH_INTROSPECTOR_IMPL_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_GRAPH_INTROSPECTOR_IMPL_H_

#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/resource_coordinator/public/mojom/coordination_unit_introspector.mojom.h"
#include "services/service_manager/public/cpp/bind_source_info.h"

namespace service_manager {
struct BindSourceInfo;
}  // namespace service_manager

namespace performance_manager {

class Graph;

class CoordinationUnitIntrospectorImpl
    : public resource_coordinator::mojom::CoordinationUnitIntrospector {
 public:
  explicit CoordinationUnitIntrospectorImpl(Graph* graph);
  ~CoordinationUnitIntrospectorImpl() override;

  void BindToInterface(
      resource_coordinator::mojom::CoordinationUnitIntrospectorRequest request,
      const service_manager::BindSourceInfo& source_info);

  // Overridden from resource_coordinator::mojom::CoordinationUnitIntrospector:
  void GetProcessToURLMap(GetProcessToURLMapCallback callback) override;

 private:
  Graph* const graph_;
  mojo::BindingSet<resource_coordinator::mojom::CoordinationUnitIntrospector>
      bindings_;

  DISALLOW_COPY_AND_ASSIGN(CoordinationUnitIntrospectorImpl);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_GRAPH_INTROSPECTOR_IMPL_H_
