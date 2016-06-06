// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/navigation/view_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "components/mus/public/cpp/window_tree_client.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/mus/native_widget_mus.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace navigation {
namespace {

class InterstitialPageDelegate : public content::InterstitialPageDelegate {
 public:
  explicit InterstitialPageDelegate(const std::string& html) : html_(html) {}
  ~InterstitialPageDelegate() override {}
  InterstitialPageDelegate(const InterstitialPageDelegate&) = delete;
  void operator=(const InterstitialPageDelegate&) = delete;

 private:

  // content::InterstitialPageDelegate:
  std::string GetHTMLContents() override {
    return html_;
  }

  const std::string html_;
};

}  // namespace

ViewImpl::ViewImpl(shell::Connector* connector,
                   content::BrowserContext* browser_context,
                   mojom::ViewClientPtr client,
                   mojom::ViewRequest request,
                   std::unique_ptr<shell::ShellConnectionRef> ref)
    : connector_(connector),
      binding_(this, std::move(request)),
      client_(std::move(client)),
      ref_(std::move(ref)),
      web_view_(new views::WebView(browser_context)) {
  web_view_->GetWebContents()->SetDelegate(this);
}
ViewImpl::~ViewImpl() {}

void ViewImpl::NavigateTo(const GURL& url) {
  web_view_->GetWebContents()->GetController().LoadURL(
      url, content::Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
}

void ViewImpl::GoBack() {
  web_view_->GetWebContents()->GetController().GoBack();
}

void ViewImpl::GoForward() {
  web_view_->GetWebContents()->GetController().GoForward();
}

void ViewImpl::Reload(bool skip_cache) {
  if (skip_cache)
    web_view_->GetWebContents()->GetController().Reload(true);
  else
    web_view_->GetWebContents()->GetController().ReloadBypassingCache(true);
}

void ViewImpl::Stop() {
  web_view_->GetWebContents()->Stop();
}

void ViewImpl::GetWindowTreeClient(
    mus::mojom::WindowTreeClientRequest request) {
  new mus::WindowTreeClient(this, nullptr, std::move(request));
}

void ViewImpl::ShowInterstitial(const mojo::String& html) {
  content::InterstitialPage* interstitial =
      content::InterstitialPage::Create(web_view_->GetWebContents(),
                                        false,
                                        GURL(),
                                        new InterstitialPageDelegate(html));
  interstitial->Show();
}

void ViewImpl::HideInterstitial() {
  // TODO(beng): this is not quite right.
  if (web_view_->GetWebContents()->ShowingInterstitialPage())
    web_view_->GetWebContents()->GetInterstitialPage()->Proceed();
}

void ViewImpl::SetResizerSize(const gfx::Size& size) {
  resizer_size_ = size;
}

void ViewImpl::AddNewContents(content::WebContents* source,
                              content::WebContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_rect,
                              bool user_gesture,
                              bool* was_blocked) {
  mojom::ViewClientPtr client;
  mojom::ViewPtr view;
  mojom::ViewRequest view_request = GetProxy(&view);
  client_->ViewCreated(std::move(view), GetProxy(&client),
                       disposition == NEW_POPUP, initial_rect, user_gesture);
  ViewImpl* impl =
      new ViewImpl(connector_, new_contents->GetBrowserContext(),
                   std::move(client), std::move(view_request), ref_->Clone());
  // TODO(beng): This is a bit crappy. should be able to create the ViewImpl
  //             with |new_contents| instead.
  impl->web_view_->SetWebContents(new_contents);
  impl->web_view_->GetWebContents()->SetDelegate(impl);

  // TODO(beng): this reply is currently synchronous, figure out a fix.
  if (was_blocked)
    *was_blocked = false;
}

void ViewImpl::CloseContents(content::WebContents* source) {
  client_->Close();
}

void ViewImpl::LoadingStateChanged(content::WebContents* source,
                                   bool to_different_document) {
  client_->LoadingStateChanged(source->IsLoading());
}

void ViewImpl::NavigationStateChanged(content::WebContents* source,
                                      content::InvalidateTypes changed_flags) {
  client_->NavigationStateChanged(source->GetVisibleURL(),
                                  base::UTF16ToUTF8(source->GetTitle()),
                                  source->GetController().CanGoBack(),
                                  source->GetController().CanGoForward());
}

void ViewImpl::LoadProgressChanged(content::WebContents* source,
                                   double progress) {
  client_->LoadProgressChanged(progress);
}

void ViewImpl::UpdateTargetURL(content::WebContents* source, const GURL& url) {
  client_->UpdateHoverURL(url);
}

gfx::Rect ViewImpl::GetRootWindowResizerRect() const {
  gfx::Rect bounds = web_view_->GetLocalBounds();
  return gfx::Rect(bounds.right() - resizer_size_.width(),
                   bounds.bottom() - resizer_size_.height(),
                   resizer_size_.width(), resizer_size_.height());
}

void ViewImpl::OnEmbed(mus::Window* root) {
  DCHECK(!widget_.get());
  widget_.reset(new views::Widget);
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.delegate = this;
  params.native_widget = new views::NativeWidgetMus(
      widget_.get(), connector_, root, mus::mojom::SurfaceType::DEFAULT);
  widget_->Init(params);
  widget_->Show();
}

void ViewImpl::OnWindowTreeClientDestroyed(mus::WindowTreeClient* client) {}
void ViewImpl::OnEventObserved(const ui::Event& event, mus::Window* target) {}

views::View* ViewImpl::GetContentsView() {
  return web_view_;
}

views::Widget* ViewImpl::GetWidget() {
  return web_view_->GetWidget();
}

const views::Widget* ViewImpl::GetWidget() const {
  return web_view_->GetWidget();
}

}  // navigation
