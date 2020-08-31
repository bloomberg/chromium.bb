// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/captive_portal_window_proxy.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/chromeos/login/ui/captive_portal_view.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/webui/chromeos/internet_detail_dialog.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "ui/views/widget/widget.h"

namespace {

// A widget that uses the supplied Profile to return a ThemeProvider.  This is
// necessary because the Views in the captive portal UI need to access the theme
// colors; also, this widget cannot copy the theme from e.g. a Browser widget
// because there may be no Browsers started.
class CaptivePortalWidget : public views::Widget {
 public:
  explicit CaptivePortalWidget(Profile* profile);
  CaptivePortalWidget(const CaptivePortalWidget&) = delete;
  CaptivePortalWidget& operator=(const CaptivePortalWidget&) = delete;
  ~CaptivePortalWidget() override = default;

  // views::Widget:
  const ui::ThemeProvider* GetThemeProvider() const override;

 private:
  Profile* profile_;
};

CaptivePortalWidget::CaptivePortalWidget(Profile* profile)
    : profile_(profile) {}

const ui::ThemeProvider* CaptivePortalWidget::GetThemeProvider() const {
  return &ThemeService::GetThemeProviderForProfile(profile_);
}

// The captive portal dialog is system-modal, but uses the web-content-modal
// dialog manager (odd) and requires this atypical dialog widget initialization.
views::Widget* CreateWindowAsFramelessChild(Profile* profile,
                                            views::WidgetDelegate* delegate,
                                            gfx::NativeView parent) {
  views::Widget* widget = new CaptivePortalWidget(profile);

  views::Widget::InitParams params;
  params.delegate = delegate;
  params.child = true;
  params.parent = parent;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;

  widget->Init(std::move(params));
  return widget;
}

}  // namespace

namespace chromeos {

CaptivePortalWindowProxy::CaptivePortalWindowProxy(
    Delegate* delegate,
    content::WebContents* web_contents)
    : delegate_(delegate), web_contents_(web_contents) {
  DCHECK_EQ(STATE_IDLE, GetState());
}

CaptivePortalWindowProxy::~CaptivePortalWindowProxy() {
  if (!widget_)
    return;
  DCHECK_EQ(STATE_DISPLAYED, GetState());
  widget_->RemoveObserver(this);
  widget_->Close();
}

void CaptivePortalWindowProxy::ShowIfRedirected() {
  if (GetState() != STATE_IDLE)
    return;
  InitCaptivePortalView();
  DCHECK_EQ(STATE_WAITING_FOR_REDIRECTION, GetState());
}

void CaptivePortalWindowProxy::Show() {
  if (InternetDetailDialog::IsShown()) {
    // InternetDetailDialog is being shown, don't cover it.
    Close();
    return;
  }

  if (GetState() == STATE_DISPLAYED)  // Dialog is already shown, do nothing.
    return;

  for (auto& observer : observers_)
    observer.OnBeforeCaptivePortalShown();

  InitCaptivePortalView();

  CaptivePortalView* portal = captive_portal_view_.release();
  auto* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents_);
  widget_ = CreateWindowAsFramelessChild(
      profile_, portal,
      manager->delegate()->GetWebContentsModalDialogHost()->GetHostView());
  portal->Init();
  widget_->AddObserver(this);
  constrained_window::ShowModalDialog(widget_->GetNativeView(), web_contents_);
}

void CaptivePortalWindowProxy::Close() {
  if (GetState() == STATE_DISPLAYED)
    widget_->Close();
  captive_portal_view_.reset();
  captive_portal_view_for_testing_ = nullptr;
}

void CaptivePortalWindowProxy::OnRedirected() {
  if (GetState() == STATE_WAITING_FOR_REDIRECTION) {
    if (!started_loading_at_.is_null()) {
      UMA_HISTOGRAM_TIMES("CaptivePortal.RedirectTime",
                          base::Time::Now() - started_loading_at_);
      started_loading_at_ = base::Time();
    }
    Show();
  }
  delegate_->OnPortalDetected();
}

void CaptivePortalWindowProxy::OnOriginalURLLoaded() {
  Close();
}

void CaptivePortalWindowProxy::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CaptivePortalWindowProxy::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void CaptivePortalWindowProxy::OnWidgetDestroyed(views::Widget* widget) {
  DCHECK_EQ(STATE_DISPLAYED, GetState());
  DCHECK_EQ(widget, widget_);

  DetachFromWidget(widget);

  DCHECK_EQ(STATE_IDLE, GetState());

  for (auto& observer : observers_)
    observer.OnAfterCaptivePortalHidden();
}

void CaptivePortalWindowProxy::InitCaptivePortalView() {
  DCHECK(GetState() == STATE_IDLE ||
         GetState() == STATE_WAITING_FOR_REDIRECTION);
  if (!captive_portal_view_.get()) {
    captive_portal_view_ = std::make_unique<CaptivePortalView>(profile_, this);
    captive_portal_view_for_testing_ = captive_portal_view_.get();
  }

  started_loading_at_ = base::Time::Now();
  captive_portal_view_->StartLoad();
}

CaptivePortalWindowProxy::State CaptivePortalWindowProxy::GetState() const {
  if (!widget_)
    return captive_portal_view_ ? STATE_WAITING_FOR_REDIRECTION : STATE_IDLE;
  DCHECK(!captive_portal_view_);
  return STATE_DISPLAYED;
}

void CaptivePortalWindowProxy::DetachFromWidget(views::Widget* widget) {
  if (!widget_ || widget_ != widget)
    return;
  widget_->RemoveObserver(this);
  widget_ = nullptr;
}

}  // namespace chromeos
