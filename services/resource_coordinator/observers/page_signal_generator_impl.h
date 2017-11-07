// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_PAGE_SIGNAL_GENERATOR_IMPL_H_
#define SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_PAGE_SIGNAL_GENERATOR_IMPL_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "services/resource_coordinator/observers/coordination_unit_graph_observer.h"
#include "services/resource_coordinator/public/interfaces/page_signal.mojom.h"

namespace service_manager {
struct BindSourceInfo;
}  // namespace service_manager

namespace resource_coordinator {

// The PageSignalGenerator is a dedicated |CoordinationUnitGraphObserver| for
// calculating and emitting page-scoped signals. This observer observes
// ProcessCoordinationUnits and FrameCoordinationUnits, utilize information from
// the graph and generate page level signals.
class PageSignalGeneratorImpl : public CoordinationUnitGraphObserver,
                                public mojom::PageSignalGenerator {
 public:
  PageSignalGeneratorImpl();
  ~PageSignalGeneratorImpl() override;

  // mojom::PageSignalGenerator implementation.
  void AddReceiver(mojom::PageSignalReceiverPtr receiver) override;

  // CoordinationUnitGraphObserver implementation.
  bool ShouldObserve(const CoordinationUnitBase* coordination_unit) override;
  void OnFramePropertyChanged(const FrameCoordinationUnitImpl* frame_cu,
                              const mojom::PropertyType property_type,
                              int64_t value) override;
  void OnProcessPropertyChanged(const ProcessCoordinationUnitImpl* process_cu,
                                const mojom::PropertyType property_type,
                                int64_t value) override;

  void BindToInterface(
      resource_coordinator::mojom::PageSignalGeneratorRequest request,
      const service_manager::BindSourceInfo& source_info);

 private:
  void NotifyPageAlmostIdleIfPossible(
      const FrameCoordinationUnitImpl* frame_cu);

  mojo::BindingSet<mojom::PageSignalGenerator> bindings_;
  mojo::InterfacePtrSet<mojom::PageSignalReceiver> receivers_;

  DISALLOW_COPY_AND_ASSIGN(PageSignalGeneratorImpl);
};

}  // namespace resource_coordinator

#endif  // SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_PAGE_SIGNAL_GENERATOR_IMPL_H_
