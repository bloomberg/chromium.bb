// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_BROWSER_CONTROLLER_IMPL_H_
#define WEBLAYER_BROWSER_BROWSER_CONTROLLER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "weblayer/public/browser_controller.h"

#if defined(OS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

namespace content {
class WebContents;
}

namespace weblayer {
class FullscreenDelegate;
class NavigationControllerImpl;
class NewBrowserDelegate;
class ProfileImpl;

#if defined(OS_ANDROID)
class TopControlsContainerView;
#endif

class BrowserControllerImpl : public BrowserController,
                              public content::WebContentsDelegate,
                              public content::WebContentsObserver {
 public:
  // TODO(sky): investigate a better way to not have so many ifdefs.
#if defined(OS_ANDROID)
  BrowserControllerImpl(ProfileImpl* profile,
                        const base::android::JavaParamRef<jobject>& java_impl);
#endif
  explicit BrowserControllerImpl(
      ProfileImpl* profile,
      std::unique_ptr<content::WebContents> = nullptr);
  ~BrowserControllerImpl() override;

  // Returns the BrowserControllerImpl from the specified WebContents, or null
  // if BrowserControllerImpl was not created by a BrowserControllerImpl.
  static BrowserControllerImpl* FromWebContents(
      content::WebContents* web_contents);

  content::WebContents* web_contents() const { return web_contents_.get(); }

  bool has_new_browser_delegate() const {
    return new_browser_delegate_ != nullptr;
  }

#if defined(OS_ANDROID)
  base::android::ScopedJavaLocalRef<jobject> GetWebContents(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void SetTopControlsContainerView(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller,
      jlong native_top_controls_container_view);
  void ExecuteScript(JNIEnv* env,
                     const base::android::JavaParamRef<jstring>& script,
                     bool use_separate_isolate,
                     const base::android::JavaParamRef<jobject>& callback);
  void SetJavaImpl(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& impl);
#endif

  DownloadDelegate* download_delegate() { return download_delegate_; }
  FullscreenDelegate* fullscreen_delegate() { return fullscreen_delegate_; }

  // BrowserController:
  void SetDownloadDelegate(DownloadDelegate* delegate) override;
  void SetFullscreenDelegate(FullscreenDelegate* delegate) override;
  void SetNewBrowserDelegate(NewBrowserDelegate* delegate) override;
  void AddObserver(BrowserObserver* observer) override;
  void RemoveObserver(BrowserObserver* observer) override;
  NavigationController* GetNavigationController() override;
  void ExecuteScript(const base::string16& script,
                     bool use_separate_isolate,
                     JavaScriptResultCallback callback) override;
#if !defined(OS_ANDROID)
  void AttachToView(views::WebView* web_view) override;
#endif

 private:
  // content::WebContentsDelegate:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  void DidNavigateMainFramePostCommit(
      content::WebContents* web_contents) override;
  content::ColorChooser* OpenColorChooser(
      content::WebContents* web_contents,
      SkColor color,
      const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions)
      override;
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      std::unique_ptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;
  int GetTopControlsHeight() override;
  bool DoBrowserControlsShrinkRendererSize(
      const content::WebContents* web_contents) override;
  bool EmbedsFullscreenWidget() override;
  void EnterFullscreenModeForTab(
      content::WebContents* web_contents,
      const GURL& origin,
      const blink::mojom::FullscreenOptions& options) override;
  void ExitFullscreenModeForTab(content::WebContents* web_contents) override;
  bool IsFullscreenForTabOrPending(
      const content::WebContents* web_contents) override;
  blink::mojom::DisplayMode GetDisplayMode(
      const content::WebContents* web_contents) override;
  void AddNewContents(content::WebContents* source,
                      std::unique_ptr<content::WebContents> new_contents,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_rect,
                      bool user_gesture,
                      bool* was_blocked) override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Called from closure supplied to delegate to exit fullscreen.
  void OnExitFullscreen();

  DownloadDelegate* download_delegate_ = nullptr;
  FullscreenDelegate* fullscreen_delegate_ = nullptr;
  NewBrowserDelegate* new_browser_delegate_ = nullptr;
  ProfileImpl* profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<NavigationControllerImpl> navigation_controller_;
  base::ObserverList<BrowserObserver>::Unchecked observers_;
#if defined(OS_ANDROID)
  TopControlsContainerView* top_controls_container_view_ = nullptr;
  base::android::ScopedJavaGlobalRef<jobject> java_impl_;
#endif

  bool is_fullscreen_ = false;
  // Set to true doing EnterFullscreenModeForTab().
  bool processing_enter_fullscreen_ = false;

  base::WeakPtrFactory<BrowserControllerImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BrowserControllerImpl);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_BROWSER_CONTROLLER_IMPL_H_
