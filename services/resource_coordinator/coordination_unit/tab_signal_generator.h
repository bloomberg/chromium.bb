// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_TAB_SIGNAL_GENERATOR_H_
#define SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_TAB_SIGNAL_GENERATOR_H_

#include "base/macros.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_graph_observer.h"

namespace resource_coordinator {

class CoordinationUnitImpl;

// The TabSignalGenerator is a unified |CoordinationUnitGraphObserver| for
// calculating and emitting tab-scoped signals. Nonetheless, the observer
// has access to the whole coordination unit graph and thus can utilize
// the information contained within the entire graph to generate the signals.
class TabSignalGenerator : public CoordinationUnitGraphObserver {
 public:
  TabSignalGenerator();
  ~TabSignalGenerator() override;

  bool ShouldObserve(const CoordinationUnitImpl* coordination_unit) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TabSignalGenerator);
};

}  // namespace resource_coordinator

#endif  // SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_TAB_SIGNAL_GENERATOR_H_
