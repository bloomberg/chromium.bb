// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/navigation_interception/navigation_params_android.h"

#include "components/navigation_interception/jni_headers/NavigationParams_jni.h"
#include "url/android/gurl_android.h"

namespace navigation_interception {

base::android::ScopedJavaLocalRef<jobject> CreateJavaNavigationParams(
    JNIEnv* env,
    const NavigationParams& params,
    bool has_user_gesture_carryover) {
  const GURL& url = params.base_url_for_data_url().is_empty()
                        ? params.url()
                        : params.base_url_for_data_url();

  return Java_NavigationParams_create(
      env, url::GURLAndroid::FromNativeGURL(env, url),
      url::GURLAndroid::FromNativeGURL(env, params.referrer().url),
      params.navigation_id(), params.is_post(), params.has_user_gesture(),
      params.transition_type(), params.is_redirect(),
      params.is_external_protocol(), params.is_main_frame(),
      params.is_renderer_initiated(), has_user_gesture_carryover,
      params.initiator_origin() ? params.initiator_origin()->CreateJavaObject()
                                : nullptr);
}

}  // namespace navigation_interception
