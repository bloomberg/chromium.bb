// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_GRAPH_PAGE_NODE_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_GRAPH_PAGE_NODE_H_

#include <string>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "chrome/browser/performance_manager/public/graph/node.h"
#include "chrome/browser/performance_manager/public/web_contents_proxy.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/resource_coordinator/public/mojom/coordination_unit.mojom.h"
#include "services/resource_coordinator/public/mojom/lifecycle.mojom.h"

class GURL;

namespace performance_manager {

class FrameNode;
class PageNodeObserver;

// A PageNode represents the root of a FrameTree, or equivalently a WebContents.
// These may correspond to normal tabs, WebViews, Portals, Chrome Apps or
// Extensions.
class PageNode : public Node {
 public:
  using InterventionPolicy = resource_coordinator::mojom::InterventionPolicy;
  using LifecycleState = resource_coordinator::mojom::LifecycleState;
  using Observer = PageNodeObserver;
  class ObserverDefaultImpl;

  PageNode();
  ~PageNode() override;

  // Returns the unique ID of the browser context that this page belongs to.
  virtual const std::string& GetBrowserContextID() const = 0;

  // Returns the page almost idle state of this page.
  // See PageNodeObserver::OnPageAlmostIdleChanged.
  virtual bool IsPageAlmostIdle() const = 0;

  // Returns true if this page is currently visible, false otherwise.
  // See PageNodeObserver::OnIsVisibleChanged.
  virtual bool IsVisible() const = 0;

  // Returns the time since the last visibility change. It is always well
  // defined as the visibility property is set at node creation.
  virtual base::TimeDelta GetTimeSinceLastVisibilityChange() const = 0;

  // Returns true if this page is currently audible, false otherwise.
  // See PageNodeObserver::OnIsAudibleChanged.
  virtual bool IsAudible() const = 0;

  // Returns true if this page is currently loading, false otherwise.
  // See PageNodeObserver::OnIsLoadingChanged.
  virtual bool IsLoading() const = 0;

  // Returns the UKM source ID associated with the URL of the main frame of
  // this page.
  // See PageNodeObserver::OnUkmSourceIdChanged.
  virtual ukm::SourceId GetUkmSourceID() const = 0;

  // Returns the lifecycle state of this page. This is aggregated from the
  // lifecycle state of each frame in the frame tree. See
  // PageNodeObserver::OnPageLifecycleStateChanged.
  virtual LifecycleState GetLifecycleState() const = 0;

  // Returns the freeze policy set via origin trial.
  virtual InterventionPolicy GetOriginTrialFreezePolicy() const = 0;

  // Returns the navigation ID associated with the last committed navigation
  // event for the main frame of this page.
  // See PageNodeObserver::OnMainFrameNavigationCommitted.
  virtual int64_t GetNavigationID() const = 0;

  // Returns "zero" if no navigation has happened, otherwise returns the time
  // since the last navigation commit.
  virtual base::TimeDelta GetTimeSinceLastNavigation() const = 0;

  // Returns the current main frame node (if there is one), otherwise returns
  // any of the potentially multiple main frames that currently exist. If there
  // are no main frames at the moment, returns nullptr.
  virtual const FrameNode* GetMainFrameNode() const = 0;

  // Returns all of the main frame nodes, both current and otherwise. If there
  // are no main frames at the moment, returns the empty set.
  virtual const base::flat_set<const FrameNode*> GetMainFrameNodes() const = 0;

  // Returns the URL the main frame last committed a navigation to, or the
  // initial URL of the page before navigation. The latter case is distinguished
  // by a zero navigation ID.
  // See PageNodeObserver::OnMainFrameNavigationCommitted.
  virtual const GURL& GetMainFrameUrl() const = 0;

  // Returns the web contents associated with this page node. It is valid to
  // call this function on any thread but the weak pointer must only be
  // dereferenced on the UI thread.
  virtual const WebContentsProxy& GetContentsProxy() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(PageNode);
};

// Pure virtual observer interface. Derive from this if you want to be forced to
// implement the entire interface.
class PageNodeObserver {
 public:
  PageNodeObserver();
  virtual ~PageNodeObserver();

  // Node lifetime notifications.

  // Called when a |page_node| is added to the graph.
  virtual void OnPageNodeAdded(const PageNode* page_node) = 0;

  // Called before a |page_node| is removed from the graph.
  virtual void OnBeforePageNodeRemoved(const PageNode* page_node) = 0;

  // Notifications of property changes.

  // Invoked when the IsVisible property changes.
  virtual void OnIsVisibleChanged(const PageNode* page_node) = 0;

  // Invoked when the IsAudible property changes.
  virtual void OnIsAudibleChanged(const PageNode* page_node) = 0;

  // Invoked when the IsLoading property changes.
  virtual void OnIsLoadingChanged(const PageNode* page_node) = 0;

  // Invoked when the UkmSourceId property changes.
  virtual void OnUkmSourceIdChanged(const PageNode* page_node) = 0;

  // Invoked when the PageLifecycleState property changes.
  virtual void OnPageLifecycleStateChanged(const PageNode* page_node) = 0;

  // Invoked when the OriginTrialFreezePolicy property changes.
  virtual void OnPageOriginTrialFreezePolicyChanged(
      const PageNode* page_node) = 0;

  // Invoked when the PageAlmostIdle property changes.
  virtual void OnPageAlmostIdleChanged(const PageNode* page_node) = 0;

  // This is fired when a main frame navigation commits. It indicates that the
  // |main_frame_url| and possibly the |navigation_id| properties have changed.
  // Prior to loading and initial navigation of page, the |main_frame_url| can
  // change, while the |navigation_id| stays zero.
  // TODO(siggi): Maybe #hash navigation can work the same way, or perhaps the
  //     two each properties deserve their own notification?
  virtual void OnMainFrameNavigationCommitted(const PageNode* page_node) = 0;

  // Events with no property changes.

  // Fired when the tab title associated with a page changes. This property is
  // not directly reflected on the node.
  virtual void OnTitleUpdated(const PageNode* page_node) = 0;

  // Fired when the favicon associated with a page is updated. This property is
  // not directly reflected on the node.
  virtual void OnFaviconUpdated(const PageNode* page_node) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(PageNodeObserver);
};

// Default implementation of observer that provides dummy versions of each
// function. Derive from this if you only need to implement a few of the
// functions.
class PageNode::ObserverDefaultImpl : public PageNodeObserver {
 public:
  ObserverDefaultImpl();
  ~ObserverDefaultImpl() override;

  // PageNodeObserver implementation:
  void OnPageNodeAdded(const PageNode* page_node) override {}
  void OnBeforePageNodeRemoved(const PageNode* page_node) override {}
  void OnIsVisibleChanged(const PageNode* page_node) override {}
  void OnIsAudibleChanged(const PageNode* page_node) override {}
  void OnIsLoadingChanged(const PageNode* page_node) override {}
  void OnUkmSourceIdChanged(const PageNode* page_node) override {}
  void OnPageLifecycleStateChanged(const PageNode* page_node) override {}
  void OnPageOriginTrialFreezePolicyChanged(
      const PageNode* page_node) override {}
  void OnPageAlmostIdleChanged(const PageNode* page_node) override {}
  void OnMainFrameNavigationCommitted(const PageNode* page_node) override {}
  void OnTitleUpdated(const PageNode* page_node) override {}
  void OnFaviconUpdated(const PageNode* page_node) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ObserverDefaultImpl);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_GRAPH_PAGE_NODE_H_
