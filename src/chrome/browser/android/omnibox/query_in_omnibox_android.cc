// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/ui/omnibox/query_in_omnibox_factory.h"
#include "components/omnibox/browser/query_in_omnibox.h"
#include "jni/QueryInOmnibox_jni.h"

using base::android::JavaParamRef;

static base::android::ScopedJavaLocalRef<jstring>
JNI_QueryInOmnibox_GetDisplaySearchTerms(JNIEnv* env,
                                         const JavaParamRef<jclass>&,
                                         const JavaParamRef<jobject>& j_profile,
                                         jint j_security_level,
                                         const JavaParamRef<jstring>& j_url) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  security_state::SecurityLevel security_level =
      static_cast<security_state::SecurityLevel>(j_security_level);
  GURL url = GURL(base::android::ConvertJavaStringToUTF16(env, j_url));

  base::string16 search_terms;
  bool should_display =
      QueryInOmniboxFactory::GetForProfile(profile)->GetDisplaySearchTerms(
          security_level, url, &search_terms);

  if (!should_display)
    return nullptr;

  return base::android::ConvertUTF16ToJavaString(env, search_terms);
}

void JNI_QueryInOmnibox_SetIgnoreSecurityLevel(
    JNIEnv* env,
    const JavaParamRef<jclass>&,
    const JavaParamRef<jobject>& j_profile,
    jboolean ignore) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  QueryInOmniboxFactory::GetForProfile(profile)->set_ignore_security_level(
      ignore);
}
