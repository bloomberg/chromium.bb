// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/browser_controller_impl.h"

#include "base/auto_reset.h"
#include "base/logging.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/browser_controls_state.h"
#include "weblayer/browser/navigation_controller_impl.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/public/browser_observer.h"
#include "weblayer/public/fullscreen_delegate.h"

#if !defined(OS_ANDROID)
#include "ui/views/controls/webview/webview.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/jni_string.h"
#include "weblayer/browser/java/jni/BrowserControllerImpl_jni.h"
#include "weblayer/browser/top_controls_container_view.h"
#endif

namespace weblayer {

namespace {

// Pointer value of this is used as a key in base::SupportsUserData for
// WebContents. Value of the key is an instance of |UserData|.
constexpr int kWebContentsUserDataKey = 0;

struct UserData : public base::SupportsUserData::Data {
  BrowserControllerImpl* controller = nullptr;
};

#if defined(OS_ANDROID)
BrowserController* g_last_browser_controller;
#endif

}  // namespace

BrowserControllerImpl::BrowserControllerImpl(ProfileImpl* profile)
    : profile_(profile) {
#if defined(OS_ANDROID)
  g_last_browser_controller = this;
#endif
  content::WebContents::CreateParams create_params(
      profile_->GetBrowserContext());
  web_contents_ = content::WebContents::Create(create_params);
  std::unique_ptr<UserData> user_data = std::make_unique<UserData>();
  user_data->controller = this;
  web_contents_->SetUserData(&kWebContentsUserDataKey, std::move(user_data));

  web_contents_->SetDelegate(this);
  Observe(web_contents_.get());

  navigation_controller_ = std::make_unique<NavigationControllerImpl>(this);
}

BrowserControllerImpl::~BrowserControllerImpl() {
  // Destruct this now to avoid it calling back when this object is partially
  // destructed.
  web_contents_.reset();
}

// static
BrowserControllerImpl* BrowserControllerImpl::FromWebContents(
    content::WebContents* web_contents) {
  return reinterpret_cast<UserData*>(
             web_contents->GetUserData(&kWebContentsUserDataKey))
      ->controller;
}

void BrowserControllerImpl::SetFullscreenDelegate(
    FullscreenDelegate* delegate) {
  if (delegate == fullscreen_delegate_)
    return;

  const bool had_delegate = (fullscreen_delegate_ != nullptr);
  const bool has_delegate = (delegate != nullptr);

  // If currently fullscreen, and the delegate is being set to null, force an
  // exit now (otherwise the delegate can't take us out of fullscreen).
  if (is_fullscreen_ && fullscreen_delegate_ && had_delegate != has_delegate)
    OnExitFullscreen();

  fullscreen_delegate_ = delegate;
  // Whether fullscreen is enabled depends upon whether there is a delegate. If
  // having a delegate changed, then update the renderer (which is where
  // fullscreen enabled is tracked).
  content::RenderViewHost* host = web_contents_->GetRenderViewHost();
  if (had_delegate != has_delegate && host)
    host->OnWebkitPreferencesChanged();
}

void BrowserControllerImpl::AddObserver(BrowserObserver* observer) {
  observers_.AddObserver(observer);
}

void BrowserControllerImpl::RemoveObserver(BrowserObserver* observer) {
  observers_.RemoveObserver(observer);
}

NavigationController* BrowserControllerImpl::GetNavigationController() {
  return navigation_controller_.get();
}

#if !defined(OS_ANDROID)
void BrowserControllerImpl::AttachToView(views::WebView* web_view) {
  web_view->SetWebContents(web_contents_.get());
  web_contents_->Focus();
}
#endif

#if defined(OS_ANDROID)
static jlong JNI_BrowserControllerImpl_CreateBrowserController(JNIEnv* env,
                                                               jlong profile) {
  return reinterpret_cast<intptr_t>(
      new BrowserControllerImpl(reinterpret_cast<ProfileImpl*>(profile)));
}

static void JNI_BrowserControllerImpl_DeleteBrowserController(
    JNIEnv* env,
    jlong browser_controller) {
  delete reinterpret_cast<BrowserControllerImpl*>(browser_controller);
}

base::android::ScopedJavaLocalRef<jobject>
BrowserControllerImpl::GetWebContents(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return web_contents_->GetJavaWebContents();
}

void BrowserControllerImpl::SetTopControlsContainerView(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    jlong native_top_controls_container_view) {
  top_controls_container_view_ = reinterpret_cast<TopControlsContainerView*>(
      native_top_controls_container_view);
}

#endif

void BrowserControllerImpl::LoadingStateChanged(content::WebContents* source,
                                                bool to_different_document) {
  bool is_loading = web_contents_->IsLoading();
  for (auto& observer : observers_)
    observer.LoadingStateChanged(is_loading, to_different_document);
}

void BrowserControllerImpl::LoadProgressChanged(content::WebContents* source,
                                                double progress) {
  for (auto& observer : observers_)
    observer.LoadProgressChanged(progress);
}

void BrowserControllerImpl::DidNavigateMainFramePostCommit(
    content::WebContents* web_contents) {
  for (auto& observer : observers_)
    observer.DisplayedUrlChanged(web_contents->GetVisibleURL());
}

int BrowserControllerImpl::GetTopControlsHeight() {
#if defined(OS_ANDROID)
  return top_controls_container_view_->GetTopControlsHeight();
#else
  return 0;
#endif
}

bool BrowserControllerImpl::DoBrowserControlsShrinkRendererSize(
    const content::WebContents* web_contents) {
  return true;
}

bool BrowserControllerImpl::EmbedsFullscreenWidget() {
  return true;
}

void BrowserControllerImpl::EnterFullscreenModeForTab(
    content::WebContents* web_contents,
    const GURL& origin,
    const blink::mojom::FullscreenOptions& options) {
  // TODO: support |options|.
  is_fullscreen_ = true;
  auto exit_fullscreen_closure = base::BindOnce(
      &BrowserControllerImpl::OnExitFullscreen, weak_ptr_factory_.GetWeakPtr());
  base::AutoReset<bool> reset(&processing_enter_fullscreen_, true);
  fullscreen_delegate_->EnterFullscreen(std::move(exit_fullscreen_closure));
}

void BrowserControllerImpl::ExitFullscreenModeForTab(
    content::WebContents* web_contents) {
  is_fullscreen_ = false;
  fullscreen_delegate_->ExitFullscreen();
}

bool BrowserControllerImpl::IsFullscreenForTabOrPending(
    const content::WebContents* web_contents) {
  return is_fullscreen_;
}

blink::mojom::DisplayMode BrowserControllerImpl::GetDisplayMode(
    const content::WebContents* web_contents) {
  return is_fullscreen_ ? blink::mojom::DisplayMode::kFullscreen
                        : blink::mojom::DisplayMode::kBrowser;
}

void BrowserControllerImpl::DidFirstVisuallyNonEmptyPaint() {
  for (auto& observer : observers_)
    observer.FirstContentfulPaint();
}

void BrowserControllerImpl::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
#if defined(OS_ANDROID)
  web_contents_->GetMainFrame()->UpdateBrowserControlsState(
      content::BROWSER_CONTROLS_STATE_BOTH,
      content::BROWSER_CONTROLS_STATE_SHOWN, false);

  if (web_contents_->ShowingInterstitialPage()) {
    web_contents_->GetInterstitialPage()
        ->GetMainFrame()
        ->UpdateBrowserControlsState(content::BROWSER_CONTROLS_STATE_SHOWN,
                                     content::BROWSER_CONTROLS_STATE_SHOWN,
                                     false);
  }
#endif
}

void BrowserControllerImpl::OnExitFullscreen() {
  // If |processing_enter_fullscreen_| is true, it means the callback is being
  // called while processing EnterFullscreenModeForTab(). WebContents doesn't
  // deal well with this. FATAL as Android generally doesn't run with DCHECKs.
  LOG_IF(FATAL, !processing_enter_fullscreen_)
      << "exiting fullscreen while entering fullscreen is not supported";
  web_contents_->ExitFullscreen(/* will_cause_resize */ false);
}

std::unique_ptr<BrowserController> BrowserController::Create(Profile* profile) {
  return std::make_unique<BrowserControllerImpl>(
      static_cast<ProfileImpl*>(profile));
}

#if defined(OS_ANDROID)
BrowserController* BrowserController::GetLastControllerForTesting() {
  return g_last_browser_controller;
}
#endif

}  // namespace weblayer
