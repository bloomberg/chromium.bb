// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/browser_info.h"

#include "libcef/browser/browser_host_base.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/values_impl.h"

#include "base/logging.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/render_process_host.h"
#include "ipc/ipc_message.h"

CefBrowserInfo::FrameInfo::~FrameInfo() {
  if (frame_ && !is_main_frame_) {
    // Disassociate sub-frames from the browser.
    frame_->Detach();
  }
}

CefBrowserInfo::CefBrowserInfo(int browser_id,
                               bool is_popup,
                               bool is_windowless,
                               CefRefPtr<CefDictionaryValue> extra_info)
    : browser_id_(browser_id),
      is_popup_(is_popup),
      is_windowless_(is_windowless),
      extra_info_(extra_info) {
  DCHECK_GT(browser_id, 0);
}

CefBrowserInfo::~CefBrowserInfo() {}

CefRefPtr<CefBrowserHostBase> CefBrowserInfo::browser() const {
  base::AutoLock lock_scope(lock_);
  return browser_;
}

void CefBrowserInfo::SetBrowser(CefRefPtr<CefBrowserHostBase> browser) {
  base::AutoLock lock_scope(lock_);
  browser_ = browser;

  if (!browser) {
    RemoveAllFrames();
  }
}

void CefBrowserInfo::MaybeCreateFrame(content::RenderFrameHost* host,
                                      bool is_guest_view) {
  CEF_REQUIRE_UIT();

  const auto frame_id = CefFrameHostImpl::MakeFrameId(host);
  const int frame_tree_node_id = host->GetFrameTreeNodeId();
  const bool is_main_frame = (host->GetParent() == nullptr);

  // A speculative RFH will be created in response to a browser-initiated
  // cross-origin navigation (e.g. via LoadURL) and eventually either discarded
  // or swapped in based on whether the navigation is committed. We'll create a
  // frame object for the speculative RFH so that it can be found by
  // frame/routing ID. However, we won't replace the main frame with a
  // speculative RFH until after it's swapped in, and we'll generally prefer to
  // return a non-speculative RFH for the same node ID if one exists.
  const bool is_speculative = (static_cast<content::RenderFrameHostImpl*>(host)
                                   ->frame_tree_node()
                                   ->render_manager()
                                   ->current_frame_host() != host);

  base::AutoLock lock_scope(lock_);
  DCHECK(browser_);

  const auto it = frame_id_map_.find(frame_id);
  if (it != frame_id_map_.end()) {
    auto info = it->second;

#if DCHECK_IS_ON()
    // Check that the frame info hasn't changed unexpectedly.
    DCHECK_EQ(info->frame_id_, frame_id);
    DCHECK_EQ(info->frame_tree_node_id_, frame_tree_node_id);
    DCHECK_EQ(info->is_guest_view_, is_guest_view);
    DCHECK_EQ(info->is_main_frame_, is_main_frame);
#endif

    if (!info->is_guest_view_ && info->is_speculative_ && !is_speculative) {
      // Upgrade the frame info from speculative to non-speculative.
      if (info->is_main_frame_) {
        if (main_frame_) {
          // Update the existing main frame object.
          main_frame_->SetRenderFrameHost(host);
          info->frame_ = main_frame_;
        } else {
          // Set the main frame object.
          main_frame_ = info->frame_;
        }
      }
      info->is_speculative_ = false;
      MaybeUpdateFrameTreeNodeIdMap(info);
    }
    return;
  }

  auto frame_info = new FrameInfo;
  frame_info->host_ = host;
  frame_info->frame_id_ = frame_id;
  frame_info->frame_tree_node_id_ = frame_tree_node_id;
  frame_info->is_guest_view_ = is_guest_view;
  frame_info->is_main_frame_ = is_main_frame;
  frame_info->is_speculative_ = is_speculative;

  // Guest views don't get their own CefBrowser or CefFrame objects.
  if (!is_guest_view) {
    if (is_main_frame && main_frame_ && !is_speculative) {
      // Update the existing main frame object.
      main_frame_->SetRenderFrameHost(host);
      frame_info->frame_ = main_frame_;
    } else {
      // Create a new frame object.
      frame_info->frame_ = new CefFrameHostImpl(this, host);
      if (is_main_frame && !is_speculative) {
        main_frame_ = frame_info->frame_;
      }
    }
#if DCHECK_IS_ON()
    // Check that the frame info hasn't changed unexpectedly.
    DCHECK_EQ(frame_id, frame_info->frame_->GetIdentifier());
    DCHECK_EQ(frame_info->is_main_frame_, frame_info->frame_->IsMain());
#endif
  }

  browser_->request_context()->OnRenderFrameCreated(
      host->GetProcess()->GetID(), host->GetRoutingID(), frame_tree_node_id,
      is_main_frame, is_guest_view);

  // Populate the lookup maps.
  frame_id_map_.insert(std::make_pair(frame_id, frame_info));
  MaybeUpdateFrameTreeNodeIdMap(frame_info);

  // And finally set the ownership.
  frame_info_set_.insert(base::WrapUnique(frame_info));
}

void CefBrowserInfo::RemoveFrame(content::RenderFrameHost* host) {
  CEF_REQUIRE_UIT();

  base::AutoLock lock_scope(lock_);

  const auto frame_id = CefFrameHostImpl::MakeFrameId(host);

  auto it = frame_id_map_.find(frame_id);
  DCHECK(it != frame_id_map_.end());

  auto frame_info = it->second;

  browser_->request_context()->OnRenderFrameDeleted(
      host->GetProcess()->GetID(), host->GetRoutingID(),
      frame_info->frame_tree_node_id_, frame_info->is_main_frame_,
      frame_info->is_guest_view_);

  // Remove from the lookup maps.
  frame_id_map_.erase(it);

  // A new RFH with the same node ID may be added before the old RFH is deleted,
  // or this might be a speculative RFH. Therefore only delete the map entry if
  // it's currently pointing to the to-be-deleted frame info object.
  {
    auto it2 = frame_tree_node_id_map_.find(frame_info->frame_tree_node_id_);
    if (it2 != frame_tree_node_id_map_.end() && it2->second == frame_info) {
      frame_tree_node_id_map_.erase(frame_info->frame_tree_node_id_);
    }
  }

  // And finally delete the frame info.
  {
    auto it2 = frame_info_set_.find(frame_info);
    frame_info_set_.erase(it2);
  }
}

CefRefPtr<CefFrameHostImpl> CefBrowserInfo::GetMainFrame() {
  base::AutoLock lock_scope(lock_);
  DCHECK(browser_);
  if (!main_frame_) {
    // Create a temporary object that will eventually be updated with real
    // routing information.
    main_frame_ =
        new CefFrameHostImpl(this, true, CefFrameHostImpl::kInvalidFrameId);
  }
  return main_frame_;
}

CefRefPtr<CefFrameHostImpl> CefBrowserInfo::CreateTempSubFrame(
    int64_t parent_frame_id) {
  CefRefPtr<CefFrameHostImpl> parent = GetFrameForId(parent_frame_id);
  if (!parent)
    parent = GetMainFrame();
  return new CefFrameHostImpl(this, false, parent->GetIdentifier());
}

CefRefPtr<CefFrameHostImpl> CefBrowserInfo::GetFrameForHost(
    const content::RenderFrameHost* host,
    bool* is_guest_view) const {
  if (is_guest_view)
    *is_guest_view = false;

  if (!host)
    return nullptr;

  return GetFrameForId(CefFrameHostImpl::MakeFrameId(host), is_guest_view);
}

CefRefPtr<CefFrameHostImpl> CefBrowserInfo::GetFrameForRoute(
    int32_t render_process_id,
    int32_t render_routing_id,
    bool* is_guest_view) const {
  if (is_guest_view)
    *is_guest_view = false;

  if (render_process_id < 0 || render_routing_id < 0)
    return nullptr;

  return GetFrameForId(
      CefFrameHostImpl::MakeFrameId(render_process_id, render_routing_id),
      is_guest_view);
}

CefRefPtr<CefFrameHostImpl> CefBrowserInfo::GetFrameForId(
    int64_t frame_id,
    bool* is_guest_view) const {
  if (is_guest_view)
    *is_guest_view = false;

  if (frame_id < 0)
    return nullptr;

  base::AutoLock lock_scope(lock_);

  const auto it = frame_id_map_.find(frame_id);
  if (it != frame_id_map_.end()) {
    const auto info = it->second;

    if (info->is_guest_view_) {
      if (is_guest_view)
        *is_guest_view = true;
      return nullptr;
    }

    if (info->is_speculative_) {
      if (info->is_main_frame_ && main_frame_) {
        // Always prefer the non-speculative main frame.
        return main_frame_;
      } else {
        // Always prefer an existing non-speculative frame for the same node ID.
        bool is_guest_view_tmp;
        auto frame = GetFrameForFrameTreeNodeInternal(info->frame_tree_node_id_,
                                                      &is_guest_view_tmp);
        if (is_guest_view_tmp) {
          if (is_guest_view)
            *is_guest_view = true;
          return nullptr;
        }
        if (frame)
          return frame;
      }

      LOG(WARNING) << "Returning a speculative frame for frame id " << frame_id;
    }

    DCHECK(info->frame_);
    return info->frame_;
  }

  return nullptr;
}

CefRefPtr<CefFrameHostImpl> CefBrowserInfo::GetFrameForFrameTreeNode(
    int frame_tree_node_id,
    bool* is_guest_view) const {
  if (is_guest_view)
    *is_guest_view = false;

  if (frame_tree_node_id < 0)
    return nullptr;

  base::AutoLock lock_scope(lock_);
  return GetFrameForFrameTreeNodeInternal(frame_tree_node_id, is_guest_view);
}

CefBrowserInfo::FrameHostList CefBrowserInfo::GetAllFrames() const {
  base::AutoLock lock_scope(lock_);
  FrameHostList frames;
  for (const auto& info : frame_info_set_) {
    if (info->frame_ && !info->is_speculative_)
      frames.insert(info->frame_);
  }
  return frames;
}

CefBrowserInfo::NavigationLock::NavigationLock() : weak_ptr_factory_(this) {}

CefBrowserInfo::NavigationLock::~NavigationLock() {
  CEF_REQUIRE_UIT();
  if (pending_action_) {
    CEF_POST_TASK(CEF_UIT, std::move(pending_action_));
  }
}

scoped_refptr<CefBrowserInfo::NavigationLock>
CefBrowserInfo::CreateNavigationLock() {
  CEF_REQUIRE_UIT();
  scoped_refptr<NavigationLock> lock;
  if (!navigation_lock_) {
    lock = new NavigationLock();
    navigation_lock_ = lock->weak_ptr_factory_.GetWeakPtr();
  } else {
    lock = navigation_lock_.get();
  }
  return lock;
}

bool CefBrowserInfo::IsNavigationLocked(base::OnceClosure pending_action) {
  CEF_REQUIRE_UIT();
  if (navigation_lock_) {
    navigation_lock_->pending_action_ = std::move(pending_action);
    return true;
  }
  return false;
}

void CefBrowserInfo::MaybeUpdateFrameTreeNodeIdMap(FrameInfo* info) {
  lock_.AssertAcquired();

  auto it = frame_tree_node_id_map_.find(info->frame_tree_node_id_);
  const bool has_entry = (it != frame_tree_node_id_map_.end());

  if (has_entry && it->second == info) {
    // Already mapping to |info|.
    return;
  }

  // Don't replace an existing node ID entry with a speculative RFH, but do
  // add an entry if one doesn't already exist.
  if (!info->is_speculative_ || !has_entry) {
    // A new RFH with the same node ID may be added before the old RFH is
    // deleted. To avoid duplicate entries in the map remove the old entry, if
    // any, before adding the new entry.
    if (has_entry)
      frame_tree_node_id_map_.erase(it);

    frame_tree_node_id_map_.insert(
        std::make_pair(info->frame_tree_node_id_, info));
  }
}

CefRefPtr<CefFrameHostImpl> CefBrowserInfo::GetFrameForFrameTreeNodeInternal(
    int frame_tree_node_id,
    bool* is_guest_view) const {
  if (is_guest_view)
    *is_guest_view = false;

  lock_.AssertAcquired();

  const auto it = frame_tree_node_id_map_.find(frame_tree_node_id);
  if (it != frame_tree_node_id_map_.end()) {
    const auto info = it->second;

    LOG_IF(WARNING, info->is_speculative_)
        << "Returning a speculative frame for node id " << frame_tree_node_id;

    if (info->is_guest_view_) {
      if (is_guest_view)
        *is_guest_view = true;
      return nullptr;
    }

    DCHECK(info->frame_);
    return info->frame_;
  }

  return nullptr;
}

void CefBrowserInfo::RemoveAllFrames() {
  lock_.AssertAcquired();

  // Clear the lookup maps.
  frame_id_map_.clear();
  frame_tree_node_id_map_.clear();

  // Explicitly Detach main frames.
  for (auto& info : frame_info_set_) {
    if (info->frame_ && info->is_main_frame_)
      info->frame_->Detach();
  }

  if (main_frame_) {
    main_frame_->Detach();
    main_frame_ = nullptr;
  }

  // And finally delete the frame info.
  frame_info_set_.clear();
}
