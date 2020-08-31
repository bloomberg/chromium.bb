// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_NAVIGATION_CONTROLLER_IMPL_H_
#define WEBLAYER_BROWSER_NAVIGATION_CONTROLLER_IMPL_H_

#include <map>

#include <memory>
#include "base/macros.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "weblayer/browser/navigation_impl.h"
#include "weblayer/public/navigation_controller.h"

#if defined(OS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

namespace content {
class NavigationThrottle;
}

namespace weblayer {
class TabImpl;

class NavigationControllerImpl : public NavigationController,
                                 public content::WebContentsObserver {
 public:
  explicit NavigationControllerImpl(TabImpl* tab);
  ~NavigationControllerImpl() override;

  // Creates the NavigationThrottle used to ensure WebContents::Stop() is called
  // at safe times. See NavigationControllerImpl for details.
  std::unique_ptr<content::NavigationThrottle> CreateNavigationThrottle(
      content::NavigationHandle* handle);

#if defined(OS_ANDROID)
  void SetNavigationControllerImpl(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& java_controller);
  void Navigate(JNIEnv* env,
                const base::android::JavaParamRef<jstring>& url);
  void NavigateWithParams(JNIEnv* env,
                          const base::android::JavaParamRef<jstring>& url,
                          jboolean should_replace_current_entry);
  void GoBack(JNIEnv* env) { GoBack(); }
  void GoForward(JNIEnv* env) { GoForward(); }
  bool CanGoBack(JNIEnv* env) { return CanGoBack(); }
  bool CanGoForward(JNIEnv* env) { return CanGoForward(); }
  void GoToIndex(JNIEnv* env, int index) { return GoToIndex(index); }
  void Reload(JNIEnv* env) { Reload(); }
  void Stop(JNIEnv* env) { Stop(); }
  int GetNavigationListSize(JNIEnv* env) { return GetNavigationListSize(); }
  int GetNavigationListCurrentIndex(JNIEnv* env) {
    return GetNavigationListCurrentIndex();
  }
  base::android::ScopedJavaLocalRef<jstring> GetNavigationEntryDisplayUri(
      JNIEnv* env,
      int index);
  base::android::ScopedJavaLocalRef<jstring> GetNavigationEntryTitle(
      JNIEnv* env,
      int index);
#endif

 private:
  class NavigationThrottleImpl;

  // Called from NavigationControllerImpl::WillRedirectRequest(). See
  // description of NavigationControllerImpl for details.
  void WillRedirectRequest(NavigationThrottleImpl* throttle,
                           content::NavigationHandle* navigation_handle);

  // NavigationController implementation:
  void AddObserver(NavigationObserver* observer) override;
  void RemoveObserver(NavigationObserver* observer) override;
  void Navigate(const GURL& url) override;
  void Navigate(const GURL& url, const NavigateParams& params) override;
  void GoBack() override;
  void GoForward() override;
  bool CanGoBack() override;
  bool CanGoForward() override;
  void GoToIndex(int index) override;
  void Reload() override;
  void Stop() override;
  int GetNavigationListSize() override;
  int GetNavigationListCurrentIndex() override;
  GURL GetNavigationEntryDisplayURL(int index) override;
  std::string GetNavigationEntryTitle(int index) override;

  // content::WebContentsObserver implementation:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override;
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidStartLoading() override;
  void DidStopLoading() override;
  void LoadProgressChanged(double progress) override;
  void DidFirstVisuallyNonEmptyPaint() override;

  void NotifyLoadStateChanged();

  void DoNavigate(
      std::unique_ptr<content::NavigationController::LoadURLParams> params);

  base::ObserverList<NavigationObserver>::Unchecked observers_;
  std::map<content::NavigationHandle*, std::unique_ptr<NavigationImpl>>
      navigation_map_;

  // If non-null then processing is inside DidStartNavigation() and
  // |navigation_starting_| is the NavigationImpl that was created.
  NavigationImpl* navigation_starting_ = nullptr;

  // Set to non-null while in WillRedirectRequest().
  NavigationThrottleImpl* active_throttle_ = nullptr;

#if defined(OS_ANDROID)
  base::android::ScopedJavaGlobalRef<jobject> java_controller_;
#endif

  DISALLOW_COPY_AND_ASSIGN(NavigationControllerImpl);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_NAVIGATION_CONTROLLER_IMPL_H_
