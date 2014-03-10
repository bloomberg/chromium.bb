// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/webview/webview.h"

#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "ipc/ipc_message.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/events/event.h"
#include "ui/views/accessibility/native_view_accessibility.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/views_delegate.h"

namespace views {

// static
const char WebView::kViewClassName[] = "WebView";

////////////////////////////////////////////////////////////////////////////////
// WebView, public:

WebView::WebView(content::BrowserContext* browser_context)
    : wcv_holder_(new NativeViewHost),
      embed_fullscreen_widget_mode_enabled_(false),
      is_embedding_fullscreen_widget_(false),
      browser_context_(browser_context),
      allow_accelerators_(false) {
  AddChildView(wcv_holder_);
  NativeViewAccessibility::RegisterWebView(this);
}

WebView::~WebView() {
  NativeViewAccessibility::UnregisterWebView(this);
}

content::WebContents* WebView::GetWebContents() {
  if (!web_contents()) {
    wc_owner_.reset(CreateWebContents(browser_context_));
    wc_owner_->SetDelegate(this);
    SetWebContents(wc_owner_.get());
  }
  return web_contents();
}

void WebView::SetWebContents(content::WebContents* replacement) {
  if (replacement == web_contents())
    return;
  DetachWebContents();
  if (wc_owner_ != replacement)
    wc_owner_.reset();
  WebContentsObserver::Observe(replacement);
  // web_contents() now returns |replacement| from here onwards.
  if (embed_fullscreen_widget_mode_enabled_) {
    is_embedding_fullscreen_widget_ =
        web_contents() && web_contents()->GetFullscreenRenderWidgetHostView();
  } else {
    is_embedding_fullscreen_widget_ = false;
  }
  AttachWebContents();
}

void WebView::SetEmbedFullscreenWidgetMode(bool enable) {
  bool should_be_embedded = enable;
  if (!embed_fullscreen_widget_mode_enabled_ && enable) {
    DCHECK(!is_embedding_fullscreen_widget_);
    embed_fullscreen_widget_mode_enabled_ = true;
    should_be_embedded =
        web_contents() && web_contents()->GetFullscreenRenderWidgetHostView();
  } else if (embed_fullscreen_widget_mode_enabled_ && !enable) {
    embed_fullscreen_widget_mode_enabled_ = false;
  }
  if (should_be_embedded != is_embedding_fullscreen_widget_)
    ReattachForFullscreenChange(should_be_embedded);
}

void WebView::LoadInitialURL(const GURL& url) {
  GetWebContents()->GetController().LoadURL(
      url, content::Referrer(), content::PAGE_TRANSITION_AUTO_TOPLEVEL,
      std::string());
}

void WebView::SetFastResize(bool fast_resize) {
  wcv_holder_->set_fast_resize(fast_resize);
}

void WebView::OnWebContentsFocused(content::WebContents* web_contents) {
  FocusManager* focus_manager = GetFocusManager();
  if (focus_manager)
    focus_manager->SetFocusedView(this);
}

void WebView::SetPreferredSize(const gfx::Size& preferred_size) {
  preferred_size_ = preferred_size;
  PreferredSizeChanged();
}

////////////////////////////////////////////////////////////////////////////////
// WebView, View overrides:

const char* WebView::GetClassName() const {
  return kViewClassName;
}

void WebView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  wcv_holder_->SetSize(bounds().size());
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

bool WebView::IsFocusable() const {
  // We need to be focusable when our contents is not a view hierarchy, as
  // clicking on the contents needs to focus us.
  return !!web_contents();
}

void WebView::OnFocus() {
  if (!web_contents())
    return;
  if (is_embedding_fullscreen_widget_) {
    content::RenderWidgetHostView* const current_fs_view =
        web_contents()->GetFullscreenRenderWidgetHostView();
    if (current_fs_view)
      current_fs_view->Focus();
  } else {
    web_contents()->GetView()->Focus();
  }
}

void WebView::AboutToRequestFocusFromTabTraversal(bool reverse) {
  if (web_contents())
    web_contents()->FocusThroughTabTraversal(reverse);
}

void WebView::GetAccessibleState(ui::AXViewState* state) {
  state->role = ui::AX_ROLE_GROUP;
}

gfx::NativeViewAccessible WebView::GetNativeViewAccessible() {
  if (web_contents()) {
    content::RenderWidgetHostView* host_view =
        web_contents()->GetRenderWidgetHostView();
    if (host_view)
      return host_view->GetNativeViewAccessible();
  }
  return View::GetNativeViewAccessible();
}

gfx::Size WebView::GetPreferredSize() {
  if (preferred_size_ == gfx::Size())
    return View::GetPreferredSize();
  else
    return preferred_size_;
}

////////////////////////////////////////////////////////////////////////////////
// WebView, content::WebContentsDelegate implementation:

void WebView::WebContentsFocused(content::WebContents* web_contents) {
  DCHECK(wc_owner_.get());
  // The WebView is only the delegate of WebContentses it creates itself.
  OnWebContentsFocused(wc_owner_.get());
}

bool WebView::EmbedsFullscreenWidget() const {
  DCHECK(wc_owner_.get());
  return embed_fullscreen_widget_mode_enabled_;
}

////////////////////////////////////////////////////////////////////////////////
// WebView, content::WebContentsObserver implementation:

void WebView::RenderViewHostChanged(content::RenderViewHost* old_host,
                                    content::RenderViewHost* new_host) {
  FocusManager* const focus_manager = GetFocusManager();
  if (focus_manager && focus_manager->GetFocusedView() == this)
    OnFocus();
}

void WebView::WebContentsDestroyed(content::WebContents* web_contents) {
  // We watch for destruction of WebContents that we host but do not own. If we
  // own a WebContents that is being destroyed, we're doing the destroying, so
  // we don't want to recursively tear it down while it's being torn down.
  if (!wc_owner_.get())
    SetWebContents(NULL);
}

void WebView::DidShowFullscreenWidget(int routing_id) {
  if (embed_fullscreen_widget_mode_enabled_)
    ReattachForFullscreenChange(true);
}

void WebView::DidDestroyFullscreenWidget(int routing_id) {
  if (embed_fullscreen_widget_mode_enabled_)
    ReattachForFullscreenChange(false);
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
      web_contents()->GetView()->GetNativeView();
  if (wcv_holder_->native_view() == view_to_attach)
    return;
  wcv_holder_->Attach(view_to_attach);

  // The view will not be focused automatically when it is attached, so we need
  // to pass on focus to it if the FocusManager thinks the view is focused. Note
  // that not every Widget has a focus manager.
  FocusManager* const focus_manager = GetFocusManager();
  if (focus_manager && focus_manager->GetFocusedView() == this)
    OnFocus();

#if defined(OS_WIN)
  if (!is_embedding_fullscreen_widget_) {
    web_contents()->SetParentNativeViewAccessible(
        parent()->GetNativeViewAccessible());
  }
#endif
}

void WebView::DetachWebContents() {
  if (web_contents()) {
    wcv_holder_->Detach();
#if defined(OS_WIN)
    if (!is_embedding_fullscreen_widget_)
      web_contents()->SetParentNativeViewAccessible(NULL);
#endif
  }
}

void WebView::ReattachForFullscreenChange(bool enter_fullscreen) {
  DetachWebContents();
  is_embedding_fullscreen_widget_ = enter_fullscreen &&
      web_contents() && web_contents()->GetFullscreenRenderWidgetHostView();
  AttachWebContents();
}

content::WebContents* WebView::CreateWebContents(
      content::BrowserContext* browser_context) {
  content::WebContents* contents = NULL;
  if (ViewsDelegate::views_delegate) {
    contents = ViewsDelegate::views_delegate->CreateWebContents(
        browser_context, NULL);
  }

  if (!contents) {
    content::WebContents::CreateParams create_params(
        browser_context, NULL);
    return content::WebContents::Create(create_params);
  }

  return contents;
}

}  // namespace views
