// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_RESOURCE_COORDINATOR_SIGNAL_OBSERVER_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_RESOURCE_COORDINATOR_SIGNAL_OBSERVER_H_

#include "base/macros.h"
#include "chrome/browser/performance_manager/observers/graph_observer.h"
#include "chrome/browser/performance_manager/public/web_contents_proxy.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"

namespace resource_coordinator {

// TabManager::ResourceCoordinatorSignalObserver forwards data from the
// performance manager graph.
// TODO(chrisha): Kill this thing entirely and move all of tab manager into the
// performance manager.
class TabManager::ResourceCoordinatorSignalObserver
    : public performance_manager::GraphObserverDefaultImpl {
 public:
  using NodeBase = performance_manager::NodeBase;
  using PageNodeImpl = performance_manager::PageNodeImpl;
  using ProcessNodeImpl = performance_manager::ProcessNodeImpl;
  using WebContentsProxy = performance_manager::WebContentsProxy;

  explicit ResourceCoordinatorSignalObserver(
      const base::WeakPtr<TabManager>& tab_manager);
  ~ResourceCoordinatorSignalObserver() override;

  // GraphObserver:
  // These functions run on the performance manager sequence.
  bool ShouldObserve(const NodeBase* node) override;
  void OnPageAlmostIdleChanged(PageNodeImpl* page_node) override;
  void OnExpectedTaskQueueingDurationSample(
      ProcessNodeImpl* process_node) override;

 private:
  // Determines if a message should still be dispatched for the given
  // |tab_manager|, |contents_proxy| and |navigation_id|. If so, returns the
  // WebContents, otherwise returns nullptr. This should only be called on the
  // UI thread.
  static content::WebContents* GetContentsForDispatch(
      const base::WeakPtr<TabManager>& tab_manager,
      const WebContentsProxy& contents_proxy,
      int64_t navigation_id);

  // Equivalent to the the GraphObserver functions above, but these are the
  // counterparts that run on the UI thread.
  static void OnPageAlmostIdleOnUi(const base::WeakPtr<TabManager>& tab_manager,
                                   const WebContentsProxy& contents_proxy,
                                   int64_t navigation_id);
  static void OnExpectedTaskQueueingDurationSampleOnUi(
      const base::WeakPtr<TabManager>& tab_manager,
      const WebContentsProxy& contents_proxy,
      int64_t navigation_id,
      base::TimeDelta duration);

  // Can only be dereferenced on the UI thread. When the tab manager dies this
  // is used to drop messages received from the performance manager. Ideally
  // we'd also then tear down this observer on the perf manager sequence itself,
  // but when one dies they're both about to die.
  base::WeakPtr<TabManager> tab_manager_;

  DISALLOW_COPY_AND_ASSIGN(ResourceCoordinatorSignalObserver);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_RESOURCE_COORDINATOR_SIGNAL_OBSERVER_H_
