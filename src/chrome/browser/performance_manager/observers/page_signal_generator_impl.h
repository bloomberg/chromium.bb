// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_PAGE_SIGNAL_GENERATOR_IMPL_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_PAGE_SIGNAL_GENERATOR_IMPL_H_

#include <map>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "chrome/browser/performance_manager/observers/coordination_unit_graph_observer.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "services/resource_coordinator/public/mojom/page_signal.mojom.h"

namespace service_manager {
struct BindSourceInfo;
}  // namespace service_manager

namespace performance_manager {

// The PageSignalGenerator is a dedicated |GraphObserver| for
// calculating and emitting page-scoped signals. This observer observes
// PageCoordinationUnits, ProcessCoordinationUnits and FrameNodes,
// combining information from the graph to generate page level signals.
class PageSignalGeneratorImpl
    : public GraphObserver,
      public resource_coordinator::mojom::PageSignalGenerator {
 public:
  PageSignalGeneratorImpl();
  ~PageSignalGeneratorImpl() override;

  // resource_coordinator::mojom::PageSignalGenerator implementation.
  void AddReceiver(
      resource_coordinator::mojom::PageSignalReceiverPtr receiver) override;

  // GraphObserver implementation.
  bool ShouldObserve(const NodeBase* coordination_unit) override;
  void OnNodeCreated(NodeBase* cu) override;
  void OnBeforeNodeDestroyed(NodeBase* cu) override;
  void OnPagePropertyChanged(
      PageNodeImpl* page_node,
      resource_coordinator::mojom::PropertyType property_type,
      int64_t value) override;
  void OnProcessPropertyChanged(
      ProcessNodeImpl* process_node,
      resource_coordinator::mojom::PropertyType property_type,
      int64_t value) override;
  void OnFrameEventReceived(FrameNodeImpl* frame_node,
                            resource_coordinator::mojom::Event event) override;
  void OnProcessEventReceived(
      ProcessNodeImpl* page_node,
      resource_coordinator::mojom::Event event) override;
  void OnSystemEventReceived(SystemNodeImpl* system_node,
                             resource_coordinator::mojom::Event event) override;
  void OnPageAlmostIdleChanged(PageNodeImpl* page_node) override;

  void BindToInterface(
      resource_coordinator::mojom::PageSignalGeneratorRequest request,
      const service_manager::BindSourceInfo& source_info);

 private:
  friend class PageSignalGeneratorImplTest;
  FRIEND_TEST_ALL_PREFIXES(PageSignalGeneratorImplTest,
                           NonPersistentNotificationCreatedEvent);
  FRIEND_TEST_ALL_PREFIXES(PageSignalGeneratorImplTest,
                           PageDataCorrectlyManaged);
  FRIEND_TEST_ALL_PREFIXES(PageSignalGeneratorImplTest,
                           OnLoadTimePerformanceEstimate);

  // Holds state per page CU. These are created via OnNodeCreated
  // and destroyed via OnBeforeNodeDestroyed.
  // TODO(chrisha): Move this to a PerformanceEstimateDecorator directly on the
  // graph.
  struct PageData {
    // Sets |performance_estimate_issued| to false, and updates
    // |last_state_change| to the current time.
    void Reset();

    base::TimeTicks last_state_change;
    // True iff a performance estimate has been issued for this page.
    bool performance_estimate_issued = false;
  };

  // Convenience accessors for state associated with a |page_node|.
  PageData* GetPageData(const PageNodeImpl* page_node);

  template <typename Method, typename... Params>
  void DispatchPageSignal(const PageNodeImpl* page_node,
                          Method m,
                          Params... params);

  mojo::BindingSet<resource_coordinator::mojom::PageSignalGenerator> bindings_;
  mojo::InterfacePtrSet<resource_coordinator::mojom::PageSignalReceiver>
      receivers_;

  // Stores per Page CU data. This set is maintained by
  // OnNodeCreated and OnBeforeNodeDestroyed.
  std::map<const PageNodeImpl*, PageData> page_data_;

  DISALLOW_COPY_AND_ASSIGN(PageSignalGeneratorImpl);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_PAGE_SIGNAL_GENERATOR_IMPL_H_
