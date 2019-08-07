// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/observers/graph_observer.h"

namespace performance_manager {

GraphObserver::GraphObserver() = default;

GraphObserver::~GraphObserver() = default;

GraphObserverDefaultImpl::GraphObserverDefaultImpl() = default;

GraphObserverDefaultImpl::~GraphObserverDefaultImpl() {
  // This observer should have left the graph before being destroyed.
  DCHECK(!graph_);
}

void GraphObserverDefaultImpl::SetGraph(GraphImpl* graph) {
  // We can only be going from null to non-null, and vice-versa.
  DCHECK(!graph || !graph_);
  graph_ = graph;
}

}  // namespace performance_manager
