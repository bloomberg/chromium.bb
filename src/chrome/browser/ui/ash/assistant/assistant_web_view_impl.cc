// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/assistant_web_view_impl.h"

#include "ash/public/cpp/window_properties.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/focused_node_details.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"
#include "ui/aura/window.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

AssistantWebViewImpl::AssistantWebViewImpl(Profile* profile,
                                           const InitParams& params)
    : params_(params) {
  InitWebContents(profile);
  InitLayout(profile);
}

AssistantWebViewImpl::~AssistantWebViewImpl() {
  Observe(nullptr);
  web_contents_->SetDelegate(nullptr);
}

const char* AssistantWebViewImpl::GetClassName() const {
  return "AssistantWebViewImpl";
}

gfx::NativeView AssistantWebViewImpl::GetNativeView() {
  return web_contents_->GetNativeView();
}

void AssistantWebViewImpl::ChildPreferredSizeChanged(views::View* child) {
  DCHECK_EQ(web_view_, child);
  SetPreferredSize(web_view_->GetPreferredSize());
}

void AssistantWebViewImpl::Layout() {
  web_view_->SetBoundsRect(GetContentsBounds());
}

void AssistantWebViewImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AssistantWebViewImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool AssistantWebViewImpl::GoBack() {
  if (web_contents_->GetController().CanGoBack()) {
    web_contents_->GetController().GoBack();
    return true;
  }
  return false;
}

void AssistantWebViewImpl::Navigate(const GURL& url) {
  content::NavigationController::LoadURLParams params(url);
  web_contents_->GetController().LoadURLWithParams(params);
}

void AssistantWebViewImpl::AddedToWidget() {
  UpdateMinimizeOnBackProperty();
  AssistantWebView::AddedToWidget();
}

bool AssistantWebViewImpl::IsWebContentsCreationOverridden(
    content::SiteInstance* source_site_instance,
    content::mojom::WindowContainerType window_container_type,
    const GURL& opener_url,
    const std::string& frame_name,
    const GURL& target_url) {
  if (params_.suppress_navigation) {
    NotifyDidSuppressNavigation(target_url,
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                /*from_user_gesture=*/true);
    return true;
  }
  return content::WebContentsDelegate::IsWebContentsCreationOverridden(
      source_site_instance, window_container_type, opener_url, frame_name,
      target_url);
}

content::WebContents* AssistantWebViewImpl::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  if (params_.suppress_navigation) {
    NotifyDidSuppressNavigation(params.url, params.disposition,
                                params.user_gesture);
    return nullptr;
  }
  return content::WebContentsDelegate::OpenURLFromTab(source, params);
}

void AssistantWebViewImpl::ResizeDueToAutoResize(
    content::WebContents* web_contents,
    const gfx::Size& new_size) {
  DCHECK_EQ(web_contents_.get(), web_contents);
  web_view_->SetPreferredSize(new_size);
}

bool AssistantWebViewImpl::TakeFocus(content::WebContents* web_contents,
                                     bool reverse) {
  DCHECK_EQ(web_contents_.get(), web_contents);
  auto* focus_manager = GetFocusManager();
  if (focus_manager)
    focus_manager->ClearNativeFocus();
  return false;
}

void AssistantWebViewImpl::NavigationStateChanged(
    content::WebContents* web_contents,
    content::InvalidateTypes changed_flags) {
  DCHECK_EQ(web_contents_.get(), web_contents);
  UpdateCanGoBack();
}

void AssistantWebViewImpl::DidStopLoading() {
  for (auto& observer : observers_)
    observer.DidStopLoading();
}

void AssistantWebViewImpl::OnFocusChangedInPage(
    content::FocusedNodeDetails* details) {
  for (auto& observer : observers_)
    observer.DidChangeFocusedNode(details->node_bounds_in_screen);
}

void AssistantWebViewImpl::RenderViewHostChanged(
    content::RenderViewHost* old_host,
    content::RenderViewHost* new_host) {
  if (!web_contents_->GetRenderWidgetHostView())
    return;

  if (!params_.enable_auto_resize)
    return;

  gfx::Size min_size(1, 1);
  if (params_.min_size)
    min_size.SetToMax(params_.min_size.value());

  gfx::Size max_size(INT_MAX, INT_MAX);
  if (params_.max_size)
    max_size.SetToMin(params_.max_size.value());

  web_contents_->GetRenderWidgetHostView()->EnableAutoResize(min_size,
                                                             max_size);
}

void AssistantWebViewImpl::NavigationEntriesDeleted() {
  UpdateCanGoBack();
}

void AssistantWebViewImpl::DidAttachInterstitialPage() {
  UpdateCanGoBack();
}

void AssistantWebViewImpl::DidDetachInterstitialPage() {
  UpdateCanGoBack();
}

void AssistantWebViewImpl::InitWebContents(Profile* profile) {
  web_contents_ =
      content::WebContents::Create(content::WebContents::CreateParams(
          profile, content::SiteInstance::Create(profile)));

  web_contents_->SetDelegate(this);
  Observe(web_contents_.get());

  // Use a transparent background.
  views::WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
      web_contents_.get(), SK_ColorTRANSPARENT);

  // If requested, suppress navigation.
  if (params_.suppress_navigation) {
    web_contents_->GetMutableRendererPrefs()
        ->browser_handles_all_top_level_requests = true;
    web_contents_->SyncRendererPrefs();
  }
}

void AssistantWebViewImpl::InitLayout(Profile* profile) {
  web_view_ = AddChildView(std::make_unique<views::WebView>(profile));
  web_view_->SetWebContents(web_contents_.get());
}

void AssistantWebViewImpl::NotifyDidSuppressNavigation(
    const GURL& url,
    WindowOpenDisposition disposition,
    bool from_user_gesture) {
  // Note that we post notification to |observers_| as an observer may cause
  // |this| to be deleted during handling of the event which is unsafe to do
  // until the original navigation sequence has been completed.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](const base::WeakPtr<AssistantWebViewImpl>& self, GURL url,
             WindowOpenDisposition disposition, bool from_user_gesture) {
            if (self) {
              for (auto& observer : self->observers_) {
                observer.DidSuppressNavigation(url, disposition,
                                               from_user_gesture);

                // We need to check |self| to confirm that |observer| did not
                // delete |this|. If |this| is deleted, we quit.
                if (!self)
                  return;
              }
            }
          },
          weak_factory_.GetWeakPtr(), url, disposition, from_user_gesture));
}

void AssistantWebViewImpl::UpdateCanGoBack() {
  const bool can_go_back = web_contents_->GetController().CanGoBack();
  if (can_go_back_ == can_go_back)
    return;

  can_go_back_ = can_go_back;

  UpdateMinimizeOnBackProperty();

  for (auto& observer : observers_)
    observer.DidChangeCanGoBack(can_go_back_);
}

void AssistantWebViewImpl::UpdateMinimizeOnBackProperty() {
  const bool minimize_on_back = params_.minimize_on_back_key && !can_go_back_;
  views::Widget* widget = GetWidget();
  if (widget) {
    widget->GetNativeWindow()->SetProperty(ash::kMinimizeOnBackKey,
                                           minimize_on_back);
  }
}
