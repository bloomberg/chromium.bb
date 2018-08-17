// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webrunner/browser/frame_impl.h"

#include <string>

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
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

}  // namespace

FrameImpl::FrameImpl(std::unique_ptr<content::WebContents> web_contents,
                     ContextImpl* context,
                     fidl::InterfaceRequest<chromium::web::Frame> frame_request)
    : web_contents_(std::move(web_contents)),
      context_(context),
      binding_(this, std::move(frame_request)) {
  binding_.set_error_handler([this]() { context_->DestroyFrame(this); });
  Observe(web_contents_.get());
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
  NOTIMPLEMENTED();
}

void FrameImpl::Reload() {
  NOTIMPLEMENTED();
}

void FrameImpl::GetVisibleEntry(GetVisibleEntryCallback callback) {
  NOTIMPLEMENTED();
  callback(nullptr);
}

void FrameImpl::DidFinishLoad(content::RenderFrameHost* render_frame_host,
                              const GURL& validated_url) {
  if (web_contents_->GetMainFrame() != render_frame_host) {
    return;
  }

  std::string current_url = validated_url.spec();
  std::string current_title = base::UTF16ToUTF8(web_contents_->GetTitle());

  bool is_changed;
  chromium::web::NavigationStateChangeDetails delta;
  if (current_title != cached_navigation_state_.title) {
    is_changed = true;
    delta.title = current_title;
  }

  if (current_url != cached_navigation_state_.url) {
    is_changed = true;
    delta.url = current_url;
  }

  binding_.events().OnNavigationStateChanged(std::move(delta));
}

}  // namespace webrunner
