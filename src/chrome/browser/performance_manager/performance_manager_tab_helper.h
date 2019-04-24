// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_TAB_HELPER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_TAB_HELPER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/performance_manager/page_resource_coordinator.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_id.h"

namespace performance_manager {

class FrameResourceCoordinator;
class PageResourceCoordinator;
class PerformanceManager;

// This tab helper maintains a page node, and its associated tree of frame nodes
// in the performance manager graph. It also sources a smattering of attributes
// into the graph, including visibility, title, and favicon bits.
// In addition it handles forwarding interface requests from the render frame
// host to the frame graph entity.
class PerformanceManagerTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PerformanceManagerTabHelper> {
 public:
  // TODO(siggi): Remove this once the PageSignalGenerator has been abolished.
  static bool GetCoordinationIDForWebContents(
      content::WebContents* web_contents,
      resource_coordinator::CoordinationUnitID* id);

  ~PerformanceManagerTabHelper() override;

  PageResourceCoordinator* page_resource_coordinator() {
    return &page_resource_coordinator_;
  }

  // WebContentsObserver overrides.
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void DidStartLoading() override;
  void DidStopLoading() override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void TitleWasSet(content::NavigationEntry* entry) override;
  void DidUpdateFaviconURL(
      const std::vector<content::FaviconURL>& candidates) override;
  void OnInterfaceRequestFromFrame(
      content::RenderFrameHost* render_frame_host,
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle* interface_pipe) override;

  void SetUkmSourceIdForTesting(ukm::SourceId id) { ukm_source_id_ = id; }

 private:
  explicit PerformanceManagerTabHelper(content::WebContents* web_contents);

  void OnMainFrameNavigation(int64_t navigation_id);
  void UpdatePageNodeVisibility(content::Visibility visibility);

  friend class content::WebContentsUserData<PerformanceManagerTabHelper>;

  // The performance manager for this process, if any.
  PerformanceManager* const performance_manager_;

  PageResourceCoordinator page_resource_coordinator_;
  ukm::SourceId ukm_source_id_ = ukm::kInvalidSourceId;

  // Favicon and title are set when a page is loaded, we only want to send
  // signals to the page node about title and favicon update from the previous
  // title and favicon, thus we want to ignore the very first update since it is
  // always supposed to happen.
  bool first_time_favicon_set_ = false;
  bool first_time_title_set_ = false;

  // Maps from RenderFrameHost to the associated RC node.
  std::map<content::RenderFrameHost*, std::unique_ptr<FrameResourceCoordinator>>
      frames_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(PerformanceManagerTabHelper);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_TAB_HELPER_H_
