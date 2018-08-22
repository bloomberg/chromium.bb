// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webrunner/browser/frame_impl.h"

#include <string>

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/platform_window/platform_window_init_properties.h"
#include "url/gurl.h"
#include "webrunner/browser/context_impl.h"

namespace webrunner {

namespace {

// Layout manager that allows only one child window and stretches it to fill the
// parent.
class LayoutManagerImpl : public aura::LayoutManager {
 public:
  LayoutManagerImpl() = default;
  ~LayoutManagerImpl() override = default;

  // aura::LayoutManager.
  void OnWindowResized() override {
    // Resize the child to match the size of the parent
    if (child_) {
      SetChildBoundsDirect(child_,
                           gfx::Rect(child_->parent()->bounds().size()));
    }
  }
  void OnWindowAddedToLayout(aura::Window* child) override {
    DCHECK(!child_);
    child_ = child;
    SetChildBoundsDirect(child_, gfx::Rect(child_->parent()->bounds().size()));
  }

  void OnWillRemoveWindowFromLayout(aura::Window* child) override {
    DCHECK_EQ(child, child_);
    child_ = nullptr;
  }

  void OnWindowRemovedFromLayout(aura::Window* child) override {}
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override {}
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override {}

 private:
  aura::Window* child_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(LayoutManagerImpl);
};

chromium::web::NavigationEntry ConvertContentNavigationEntry(
    content::NavigationEntry* entry) {
  DCHECK(entry);
  chromium::web::NavigationEntry converted;
  converted.title = base::UTF16ToUTF8(entry->GetTitle());
  converted.url = entry->GetURL().spec();
  return converted;
}

// Computes the observable differences between |entry_1| and |entry_2|.
// Returns true if they are different, |false| if their observable fields are
// identical.
bool ComputeNavigationEvent(const chromium::web::NavigationEntry& old_entry,
                            const chromium::web::NavigationEntry& new_entry,
                            chromium::web::NavigationEvent* computed_event) {
  DCHECK(computed_event);

  bool is_changed = false;

  if (old_entry.title != new_entry.title) {
    is_changed = true;
    computed_event->title = new_entry.title;
  }

  if (old_entry.url != new_entry.url) {
    is_changed = true;
    computed_event->url = new_entry.url;
  }

  return is_changed;
}

}  // namespace

FrameImpl::FrameImpl(std::unique_ptr<content::WebContents> web_contents,
                     ContextImpl* context,
                     fidl::InterfaceRequest<chromium::web::Frame> frame_request)
    : web_contents_(std::move(web_contents)),
      context_(context),
      binding_(this, std::move(frame_request)) {
  binding_.set_error_handler([this]() { context_->DestroyFrame(this); });
}

FrameImpl::~FrameImpl() {
  if (window_tree_host_) {
    window_tree_host_->Hide();
    window_tree_host_->compositor()->SetVisible(false);
  }
}

zx::unowned_channel FrameImpl::GetBindingChannelForTest() const {
  return zx::unowned_channel(binding_.channel());
}

void FrameImpl::CreateView(
    fidl::InterfaceRequest<fuchsia::ui::viewsv1token::ViewOwner> view_owner,
    fidl::InterfaceRequest<fuchsia::sys::ServiceProvider> services) {
  ui::PlatformWindowInitProperties properties;
  properties.view_owner_request = std::move(view_owner);

  window_tree_host_ =
      std::make_unique<aura::WindowTreeHostPlatform>(std::move(properties));
  window_tree_host_->InitHost();
  window_tree_host_->window()->SetLayoutManager(new LayoutManagerImpl());
  window_tree_host_->window()->AddChild(web_contents_->GetNativeView());
  window_tree_host_->window()->Show();
  window_tree_host_->Show();
  web_contents_->GetNativeView()->Show();
}

void FrameImpl::GetNavigationController(
    fidl::InterfaceRequest<chromium::web::NavigationController> controller) {
  controller_bindings_.AddBinding(this, std::move(controller));
}

void FrameImpl::LoadUrl(fidl::StringPtr url,
                        std::unique_ptr<chromium::web::LoadUrlParams> params) {
  GURL validated_url(*url);
  if (!validated_url.is_valid()) {
    DLOG(WARNING) << "Invalid URL: " << *url;
    return;
  }

  content::NavigationController::LoadURLParams params_converted(validated_url);
  params_converted.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  web_contents_->GetController().LoadURLWithParams(params_converted);
}

void FrameImpl::GoBack() {
  if (web_contents_->GetController().CanGoBack())
    web_contents_->GetController().GoBack();
}

void FrameImpl::GoForward() {
  if (web_contents_->GetController().CanGoForward())
    web_contents_->GetController().GoForward();
}

void FrameImpl::Stop() {
  web_contents_->Stop();
}

void FrameImpl::Reload(chromium::web::ReloadType type) {
  content::ReloadType internal_reload_type;
  switch (type) {
    case chromium::web::ReloadType::PARTIAL_CACHE:
      internal_reload_type = content::ReloadType::NORMAL;
      break;
    case chromium::web::ReloadType::NO_CACHE:
      internal_reload_type = content::ReloadType::BYPASSING_CACHE;
      break;
  }
  web_contents_->GetController().Reload(internal_reload_type, false);
}

void FrameImpl::GetVisibleEntry(GetVisibleEntryCallback callback) {
  content::NavigationEntry* entry =
      web_contents_->GetController().GetVisibleEntry();
  if (!entry) {
    callback(nullptr);
    return;
  }

  chromium::web::NavigationEntry output = ConvertContentNavigationEntry(entry);
  callback(std::make_unique<chromium::web::NavigationEntry>(std::move(output)));
}

void FrameImpl::SetNavigationEventObserver(
    fidl::InterfaceHandle<chromium::web::NavigationEventObserver> observer) {
  // Reset the event buffer state.
  waiting_for_navigation_event_ack_ = false;
  cached_navigation_state_ = {};
  pending_navigation_event_ = {};
  pending_navigation_event_is_dirty_ = false;

  if (observer) {
    navigation_observer_.Bind(std::move(observer));
    navigation_observer_.set_error_handler([this]() {
      // Stop observing on Observer connection loss.
      SetNavigationEventObserver(nullptr);
    });
    Observe(web_contents_.get());
  } else {
    navigation_observer_.Unbind();
    Observe(nullptr);  // Stop receiving WebContentsObserver events.
  }
}

void FrameImpl::DidFinishLoad(content::RenderFrameHost* render_frame_host,
                              const GURL& validated_url) {
  if (web_contents_->GetMainFrame() != render_frame_host) {
    return;
  }

  // TODO(kmarshall): Get NavigationEntry from NavigationController.
  chromium::web::NavigationEntry current_navigation_state;
  current_navigation_state.url = validated_url.spec();
  current_navigation_state.title = base::UTF16ToUTF8(web_contents_->GetTitle());

  pending_navigation_event_is_dirty_ |=
      ComputeNavigationEvent(cached_navigation_state_, current_navigation_state,
                             &pending_navigation_event_);
  cached_navigation_state_ = std::move(current_navigation_state);

  if (pending_navigation_event_is_dirty_ &&
      !waiting_for_navigation_event_ack_) {
    MaybeSendNavigationEvent();
  }
}

void FrameImpl::MaybeSendNavigationEvent() {
  if (pending_navigation_event_is_dirty_) {
    pending_navigation_event_is_dirty_ = false;
    waiting_for_navigation_event_ack_ = true;

    // Send the event to the observer and, upon acknowledgement, revisit this
    // function to send another update.
    navigation_observer_->OnNavigationStateChanged(
        std::move(pending_navigation_event_),
        [this]() { MaybeSendNavigationEvent(); });
    return;
  } else {
    // No more changes to report.
    waiting_for_navigation_event_ack_ = false;
  }
}

}  // namespace webrunner
