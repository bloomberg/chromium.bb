// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_NAVIGATION_HANDLE_OBSERVER_H_
#define CONTENT_PUBLIC_TEST_NAVIGATION_HANDLE_OBSERVER_H_

#include "base/macros.h"
#include "content/public/browser/navigation_handle_timing.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace content {

// Gathers data from the NavigationHandle assigned to navigations that start
// with the expected URL.
class NavigationHandleObserver : public WebContentsObserver {
 public:
  NavigationHandleObserver(WebContents* web_contents,
                           const GURL& expected_start_url);
  ~NavigationHandleObserver() override;

  void DidStartNavigation(NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

  bool has_committed() { return has_committed_; }
  bool is_error() { return is_error_; }
  bool is_main_frame() { return is_main_frame_; }
  bool is_parent_main_frame() { return is_parent_main_frame_; }
  bool is_renderer_initiated() { return is_renderer_initiated_; }
  bool is_same_document() { return is_same_document_; }
  bool was_redirected() { return was_redirected_; }
  int frame_tree_node_id() { return frame_tree_node_id_; }
  const GURL& last_committed_url() { return last_committed_url_; }
  ui::PageTransition page_transition() { return page_transition_; }
  net::Error net_error_code() { return net_error_code_; }
  int64_t navigation_id() { return navigation_id_; }
  bool is_download() { return is_download_; }
  ukm::SourceId next_page_ukm_source_id() { return next_page_ukm_source_id_; }
  base::Optional<net::AuthChallengeInfo> auth_challenge_info() {
    return auth_challenge_info_;
  }
  const net::ResolveErrorInfo& resolve_error_info() {
    return resolve_error_info_;
  }
  base::TimeTicks navigation_start() { return navigation_start_; }
  const NavigationHandleTiming& navigation_handle_timing() {
    return navigation_handle_timing_;
  }
  ReloadType reload_type() { return reload_type_; }

 private:
  // A reference to the NavigationHandle so this class will track only
  // one navigation at a time. It is set at DidStartNavigation and cleared
  // at DidFinishNavigation before the NavigationHandle is destroyed.
  NavigationHandle* handle_ = nullptr;
  bool has_committed_ = false;
  bool is_error_ = false;
  bool is_main_frame_ = false;
  bool is_parent_main_frame_ = false;
  bool is_renderer_initiated_ = true;
  bool is_same_document_ = false;
  bool was_redirected_ = false;
  int frame_tree_node_id_ = RenderFrameHost::kNoFrameTreeNodeId;
  ui::PageTransition page_transition_ = ui::PAGE_TRANSITION_LINK;
  GURL expected_start_url_;
  GURL last_committed_url_;
  net::Error net_error_code_ = net::OK;
  int64_t navigation_id_ = -1;
  bool is_download_ = false;
  ukm::SourceId next_page_ukm_source_id_ = ukm::kInvalidSourceId;
  base::Optional<net::AuthChallengeInfo> auth_challenge_info_;
  net::ResolveErrorInfo resolve_error_info_;
  base::TimeTicks navigation_start_;
  NavigationHandleTiming navigation_handle_timing_;
  ReloadType reload_type_ = ReloadType::NONE;

  DISALLOW_COPY_AND_ASSIGN(NavigationHandleObserver);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_NAVIGATION_HANDLE_OBSERVER_H_
