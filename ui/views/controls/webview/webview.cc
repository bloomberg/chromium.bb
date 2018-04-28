// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/webview/webview.h"

#include <utility>

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/events/event.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/views_delegate.h"

namespace views {

namespace {

// A testing stub that creates web contents.
WebView::WebContentsCreator* GetCreatorForTesting() {
  static base::NoDestructor<WebView::WebContentsCreator> creator;
  return creator.get();
}

}  // namespace

WebView::ScopedWebContentsCreatorForTesting::ScopedWebContentsCreatorForTesting(
    WebContentsCreator creator) {
  DCHECK(!*GetCreatorForTesting());
  *GetCreatorForTesting() = creator;
}

WebView::ScopedWebContentsCreatorForTesting::
    ~ScopedWebContentsCreatorForTesting() {
  *GetCreatorForTesting() = WebView::WebContentsCreator();
}

// static
const char WebView::kViewClassName[] = "WebView";

////////////////////////////////////////////////////////////////////////////////
// WebView, public:

WebView::WebView(content::BrowserContext* browser_context)
    : holder_(new NativeViewHost()),
      embed_fullscreen_widget_mode_enabled_(false),
      is_embedding_fullscreen_widget_(false),
      browser_context_(browser_context),
      allow_accelerators_(false) {
  AddChildView(holder_);  // Takes ownership of |holder_|.
}

WebView::~WebView() {
  SetWebContents(NULL);  // Make sure all necessary tear-down takes place.
}

content::WebContents* WebView::GetWebContents() {
  if (!web_contents()) {
    wc_owner_ = CreateWebContents(browser_context_);
    wc_owner_->SetDelegate(this);
    SetWebContents(wc_owner_.get());
  }
  return web_contents();
}

void WebView::SetWebContents(content::WebContents* replacement) {
  if (replacement == web_contents())
    return;
  SetCrashedOverlayView(nullptr);
  DetachWebContents();
  WebContentsObserver::Observe(replacement);
  // web_contents() now returns |replacement| from here onwards.
  UpdateCrashedOverlayView();
  if (wc_owner_.get() != replacement)
    wc_owner_.reset();
  if (embed_fullscreen_widget_mode_enabled_) {
    is_embedding_fullscreen_widget_ =
        web_contents() && web_contents()->GetFullscreenRenderWidgetHostView();
  } else {
    DCHECK(!is_embedding_fullscreen_widget_);
  }
  AttachWebContents();
  NotifyAccessibilityWebContentsChanged();
}

void WebView::SetEmbedFullscreenWidgetMode(bool enable) {
  DCHECK(!web_contents())
      << "Cannot change mode while a WebContents is attached.";
  embed_fullscreen_widget_mode_enabled_ = enable;
}

void WebView::LoadInitialURL(const GURL& url) {
  GetWebContents()->GetController().LoadURL(
      url, content::Referrer(), ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
      std::string());
}

void WebView::SetFastResize(bool fast_resize) {
  holder_->set_fast_resize(fast_resize);
}

void WebView::SetResizeBackgroundColor(SkColor resize_background_color) {
  holder_->set_resize_background_color(resize_background_color);
}

void WebView::SetCrashedOverlayView(View* crashed_overlay_view) {
  if (crashed_overlay_view_ == crashed_overlay_view)
    return;

  if (crashed_overlay_view_) {
    RemoveChildView(crashed_overlay_view_);
    if (!crashed_overlay_view_->owned_by_client())
      delete crashed_overlay_view_;
  }

  crashed_overlay_view_ = crashed_overlay_view;
  if (crashed_overlay_view_) {
    AddChildView(crashed_overlay_view_);
    crashed_overlay_view_->SetBoundsRect(gfx::Rect(size()));
  }

  UpdateCrashedOverlayView();
}

////////////////////////////////////////////////////////////////////////////////
// WebView, View overrides:

const char* WebView::GetClassName() const {
  return kViewClassName;
}

std::unique_ptr<content::WebContents> WebView::SwapWebContents(
    std::unique_ptr<content::WebContents> new_web_contents) {
  if (wc_owner_)
    wc_owner_->SetDelegate(NULL);
  std::unique_ptr<content::WebContents> old_web_contents(std::move(wc_owner_));
  wc_owner_ = std::move(new_web_contents);
  if (wc_owner_)
    wc_owner_->SetDelegate(this);
  SetWebContents(wc_owner_.get());
  return old_web_contents;
}

void WebView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  if (crashed_overlay_view_)
    crashed_overlay_view_->SetBoundsRect(gfx::Rect(size()));

  // In most cases, the holder is simply sized to fill this WebView's bounds.
  // Only WebContentses that are in fullscreen mode and being screen-captured
  // will engage the special layout/sizing behavior.
  gfx::Rect holder_bounds(bounds().size());
  if (!embed_fullscreen_widget_mode_enabled_ || !web_contents() ||
      !web_contents()->IsBeingCaptured() ||
      web_contents()->GetPreferredSize().IsEmpty() ||
      !(is_embedding_fullscreen_widget_ ||
        (web_contents()->GetDelegate() &&
         web_contents()->GetDelegate()->IsFullscreenForTabOrPending(
             web_contents())))) {
    // Reset the native view size.
    holder_->SetNativeViewSize(gfx::Size());
    holder_->SetBoundsRect(holder_bounds);
    if (is_letterboxing_) {
      is_letterboxing_ = false;
      OnLetterboxingChanged();
    }
    return;
  }

  // For screen-captured fullscreened content, scale the |holder_| to fit within
  // this View and center it.
  const gfx::Size capture_size = web_contents()->GetPreferredSize();
  const int64_t x =
      static_cast<int64_t>(capture_size.width()) * holder_bounds.height();
  const int64_t y =
      static_cast<int64_t>(capture_size.height()) * holder_bounds.width();
  if (y < x) {
    holder_bounds.ClampToCenteredSize(gfx::Size(
        holder_bounds.width(), static_cast<int>(y / capture_size.width())));
  } else {
    holder_bounds.ClampToCenteredSize(gfx::Size(
        static_cast<int>(x / capture_size.height()), holder_bounds.height()));
  }

  if (!is_letterboxing_) {
    is_letterboxing_ = true;
    OnLetterboxingChanged();
  }
  holder_->SetNativeViewSize(capture_size);
  holder_->SetBoundsRect(holder_bounds);
}

void WebView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.is_add)
    AttachWebContents();
}

bool WebView::SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) {
  if (allow_accelerators_)
    return FocusManager::IsTabTraversalKeyEvent(event);

  // Don't look-up accelerators or tab-traversal if we are showing a non-crashed
  // TabContents.
  // We'll first give the page a chance to process the key events.  If it does
  // not process them, they'll be returned to us and we'll treat them as
  // accelerators then.
  return web_contents() && !web_contents()->IsCrashed();
}

bool WebView::OnMousePressed(const ui::MouseEvent& event) {
  // A left-click within WebView is a request to focus.  The area within the
  // native view child is excluded since it will be handling mouse pressed
  // events itself (http://crbug.com/436192).
  if (event.IsOnlyLeftMouseButton() && HitTestPoint(event.location())) {
    gfx::Point location_in_holder = event.location();
    ConvertPointToTarget(this, holder_, &location_in_holder);
    if (!holder_->HitTestPoint(location_in_holder)) {
      RequestFocus();
      return true;
    }
  }
  return View::OnMousePressed(event);
}

void WebView::OnFocus() {
  if (web_contents() && !web_contents()->IsCrashed())
    web_contents()->Focus();
}

void WebView::AboutToRequestFocusFromTabTraversal(bool reverse) {
  if (web_contents() && !web_contents()->IsCrashed())
    web_contents()->FocusThroughTabTraversal(reverse);
}

void WebView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kWebView;
  // A webview does not need an accessible name as the document title is
  // provided via other means. Providing it here would be redundant.
  // Mark the name as explicitly empty so that accessibility_checks pass.
  node_data->SetNameExplicitlyEmpty();
}

gfx::NativeViewAccessible WebView::GetNativeViewAccessible() {
  if (web_contents() && !web_contents()->IsCrashed()) {
    content::RenderWidgetHostView* host_view =
        web_contents()->GetRenderWidgetHostView();
    if (host_view)
      return host_view->GetNativeViewAccessible();
  }
  return View::GetNativeViewAccessible();
}

////////////////////////////////////////////////////////////////////////////////
// WebView, content::WebContentsDelegate implementation:

bool WebView::EmbedsFullscreenWidget() const {
  DCHECK(wc_owner_.get());
  return embed_fullscreen_widget_mode_enabled_;
}

////////////////////////////////////////////////////////////////////////////////
// WebView, content::WebContentsObserver implementation:

void WebView::RenderViewReady() {
  UpdateCrashedOverlayView();
  NotifyAccessibilityWebContentsChanged();
}

void WebView::RenderViewDeleted(content::RenderViewHost* render_view_host) {
  UpdateCrashedOverlayView();
  NotifyAccessibilityWebContentsChanged();
}

void WebView::RenderViewHostChanged(content::RenderViewHost* old_host,
                                    content::RenderViewHost* new_host) {
  if (HasFocus())
    OnFocus();
  NotifyAccessibilityWebContentsChanged();
}

void WebView::WebContentsDestroyed() {
  NotifyAccessibilityWebContentsChanged();
}

void WebView::DidShowFullscreenWidget() {
  if (embed_fullscreen_widget_mode_enabled_)
    ReattachForFullscreenChange(true);
}

void WebView::DidDestroyFullscreenWidget() {
  if (embed_fullscreen_widget_mode_enabled_)
    ReattachForFullscreenChange(false);
}

void WebView::DidToggleFullscreenModeForTab(bool entered_fullscreen,
                                            bool will_cause_resize) {
  if (embed_fullscreen_widget_mode_enabled_)
    ReattachForFullscreenChange(entered_fullscreen);
}

void WebView::DidAttachInterstitialPage() {
  NotifyAccessibilityWebContentsChanged();
}

void WebView::DidDetachInterstitialPage() {
  NotifyAccessibilityWebContentsChanged();
}

void WebView::OnWebContentsFocused(
    content::RenderWidgetHost* render_widget_host) {
  RequestFocus();
}

void WebView::RenderProcessGone(base::TerminationStatus status) {
  UpdateCrashedOverlayView();
  NotifyAccessibilityWebContentsChanged();
}

////////////////////////////////////////////////////////////////////////////////
// WebView, private:

void WebView::AttachWebContents() {
  // Prevents attachment if the WebView isn't already in a Widget, or it's
  // already attached.
  if (!GetWidget() || !web_contents())
    return;

  const gfx::NativeView view_to_attach = is_embedding_fullscreen_widget_ ?
      web_contents()->GetFullscreenRenderWidgetHostView()->GetNativeView() :
      web_contents()->GetNativeView();
  OnBoundsChanged(bounds());
  if (holder_->native_view() == view_to_attach)
    return;

  holder_->Attach(view_to_attach);

  // The WebContents is not focused automatically when attached, so we need to
  // tell the WebContents it has focus if this has focus.
  if (HasFocus())
    OnFocus();

  OnWebContentsAttached();
}

void WebView::DetachWebContents() {
  if (web_contents())
    holder_->Detach();
}

void WebView::ReattachForFullscreenChange(bool enter_fullscreen) {
  DCHECK(embed_fullscreen_widget_mode_enabled_);
  const bool web_contents_has_separate_fs_widget =
      web_contents() && web_contents()->GetFullscreenRenderWidgetHostView();
  if (is_embedding_fullscreen_widget_ || web_contents_has_separate_fs_widget) {
    // Shutting down or starting up the embedding of the separate fullscreen
    // widget.  Need to detach and re-attach to a different native view.
    DetachWebContents();
    is_embedding_fullscreen_widget_ =
        enter_fullscreen && web_contents_has_separate_fs_widget;
    AttachWebContents();
  } else {
    // Entering or exiting "non-Flash" fullscreen mode, where the native view is
    // the same.  So, do not change attachment.
    OnBoundsChanged(bounds());
  }
  NotifyAccessibilityWebContentsChanged();
}

void WebView::UpdateCrashedOverlayView() {
  // TODO(dmazzoni): Fix WebContents::IsCrashed() so we can call that
  // instead of checking termination status codes.
  if (web_contents() &&
      web_contents()->GetCrashedStatus() !=
          base::TERMINATION_STATUS_NORMAL_TERMINATION &&
      web_contents()->GetCrashedStatus() !=
          base::TERMINATION_STATUS_STILL_RUNNING &&
      crashed_overlay_view_) {
    SetFocusBehavior(FocusBehavior::NEVER);
    crashed_overlay_view_->SetVisible(true);
    return;
  }

  SetFocusBehavior(web_contents() ? FocusBehavior::ALWAYS
                                  : FocusBehavior::NEVER);

  if (crashed_overlay_view_)
    crashed_overlay_view_->SetVisible(false);
}

void WebView::NotifyAccessibilityWebContentsChanged() {
  if (web_contents())
    NotifyAccessibilityEvent(ax::mojom::Event::kChildrenChanged, false);
}

std::unique_ptr<content::WebContents> WebView::CreateWebContents(
    content::BrowserContext* browser_context) {
  std::unique_ptr<content::WebContents> contents;
  if (*GetCreatorForTesting()) {
    contents = GetCreatorForTesting()->Run(browser_context);
  }

  if (!contents) {
    content::WebContents::CreateParams create_params(
        browser_context, NULL);
    return content::WebContents::Create(create_params);
  }

  return contents;
}

}  // namespace views
