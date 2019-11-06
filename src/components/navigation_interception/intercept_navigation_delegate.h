// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NAVIGATION_INTERCEPTION_INTERCEPT_NAVIGATION_DELEGATE_H_
#define COMPONENTS_NAVIGATION_INTERCEPTION_INTERCEPT_NAVIGATION_DELEGATE_H_

#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/macros.h"
#include "base/supports_user_data.h"
#include "components/navigation_interception/intercept_navigation_throttle.h"

namespace content {
class NavigationHandle;
class NavigationThrottle;
class WebContents;
}

namespace navigation_interception {

class NavigationParams;

// Native side of the InterceptNavigationDelegate Java interface.
// This is used to create a InterceptNavigationResourceThrottle that calls the
// Java interface method to determine whether a navigation should be ignored or
// not.
// To us this class:
// 1) the Java-side interface implementation must be associated (via the
//    Associate method) with a WebContents for which URLRequests are to be
//    intercepted,
// 2) the NavigationThrottle obtained via CreateThrottleFor must be associated
//    with the NavigationHandle in the ContentBrowserClient implementation.
class InterceptNavigationDelegate : public base::SupportsUserData::Data {
 public:
  InterceptNavigationDelegate(JNIEnv* env, jobject jdelegate);
  ~InterceptNavigationDelegate() override;

  // Associates the InterceptNavigationDelegate with a WebContents using the
  // SupportsUserData mechanism.
  // As implied by the use of scoped_ptr, the WebContents will assume ownership
  // of |delegate|.
  static void Associate(content::WebContents* web_contents,
                        std::unique_ptr<InterceptNavigationDelegate> delegate);
  // Gets the InterceptNavigationDelegate associated with the WebContents,
  // can be null.
  static InterceptNavigationDelegate* Get(content::WebContents* web_contents);

  // Creates a InterceptNavigationThrottle that will direct all callbacks to
  // the InterceptNavigationDelegate.
  static std::unique_ptr<content::NavigationThrottle> CreateThrottleFor(
      content::NavigationHandle* handle,
      navigation_interception::SynchronyMode mode);

  virtual bool ShouldIgnoreNavigation(
      const NavigationParams& navigation_params);

  // Updates |last_user_gesture_carryover_timestamp_| when user gesture is
  // carried over.
  void UpdateLastUserGestureCarryoverTimestamp();

 private:
  JavaObjectWeakGlobalRef weak_jdelegate_;
  base::TimeTicks last_user_gesture_carryover_timestamp_;

  DISALLOW_COPY_AND_ASSIGN(InterceptNavigationDelegate);
};

}  // namespace navigation_interception

#endif  // COMPONENTS_NAVIGATION_INTERCEPTION_INTERCEPT_NAVIGATION_DELEGATE_H_
