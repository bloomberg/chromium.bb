// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/navigation_interception/intercept_navigation_delegate.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/callback.h"
#include "components/navigation_interception/jni_headers/InterceptNavigationDelegate_jni.h"
#include "components/navigation_interception/navigation_params_android.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/escape.h"
#include "url/gurl.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;
using content::BrowserThread;
using ui::PageTransition;
using content::RenderViewHost;
using content::WebContents;

namespace navigation_interception {

namespace {

const int kMaxValidityOfUserGestureCarryoverInSeconds = 10;

const void* const kInterceptNavigationDelegateUserDataKey =
    &kInterceptNavigationDelegateUserDataKey;

bool CheckIfShouldIgnoreNavigationOnUIThread(WebContents* source,
                                             const NavigationParams& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(source);

  InterceptNavigationDelegate* intercept_navigation_delegate =
      InterceptNavigationDelegate::Get(source);
  if (!intercept_navigation_delegate)
    return false;

  return intercept_navigation_delegate->ShouldIgnoreNavigation(params);
}

}  // namespace

// static
void InterceptNavigationDelegate::Associate(
    WebContents* web_contents,
    std::unique_ptr<InterceptNavigationDelegate> delegate) {
  web_contents->SetUserData(kInterceptNavigationDelegateUserDataKey,
                            std::move(delegate));
}

// static
InterceptNavigationDelegate* InterceptNavigationDelegate::Get(
    WebContents* web_contents) {
  return static_cast<InterceptNavigationDelegate*>(
      web_contents->GetUserData(kInterceptNavigationDelegateUserDataKey));
}

// static
std::unique_ptr<content::NavigationThrottle>
InterceptNavigationDelegate::CreateThrottleFor(
    content::NavigationHandle* handle,
    navigation_interception::SynchronyMode mode) {
  return std::make_unique<InterceptNavigationThrottle>(
      handle, base::BindRepeating(&CheckIfShouldIgnoreNavigationOnUIThread),
      mode);
}

InterceptNavigationDelegate::InterceptNavigationDelegate(
    JNIEnv* env,
    jobject jdelegate,
    bool escape_external_handler_value)
    : weak_jdelegate_(env, jdelegate),
      escape_external_handler_value_(escape_external_handler_value) {}

InterceptNavigationDelegate::~InterceptNavigationDelegate() {
}

bool InterceptNavigationDelegate::ShouldIgnoreNavigation(
    const NavigationParams& navigation_params) {
  NavigationParams navigation_params_to_use(navigation_params);
  if (escape_external_handler_value_) {
    navigation_params_to_use.url() =
        GURL(net::EscapeExternalHandlerValue(navigation_params.url().spec()));
  }

  if (!navigation_params_to_use.url().is_valid())
    return false;

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jdelegate = weak_jdelegate_.get(env);

  if (jdelegate.is_null())
    return false;

  bool has_user_gesture_carryover =
      !navigation_params_to_use.has_user_gesture() &&
      base::TimeTicks::Now() - last_user_gesture_carryover_timestamp_ <=
          base::TimeDelta::FromSeconds(
              kMaxValidityOfUserGestureCarryoverInSeconds);

  ScopedJavaLocalRef<jobject> jobject_params = CreateJavaNavigationParams(
      env, navigation_params_to_use, has_user_gesture_carryover);

  return Java_InterceptNavigationDelegate_shouldIgnoreNavigation(
      env, jdelegate, jobject_params);
}

void InterceptNavigationDelegate::UpdateLastUserGestureCarryoverTimestamp() {
  last_user_gesture_carryover_timestamp_ = base::TimeTicks::Now();
}

}  // namespace navigation_interception
