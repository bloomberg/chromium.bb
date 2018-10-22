// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/omnibox/query_in_omnibox_android.h"

#include "base/android/jni_string.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/omnibox/browser/query_in_omnibox.h"
#include "jni/QueryInOmnibox_jni.h"

using base::android::JavaParamRef;

QueryInOmniboxAndroid::QueryInOmniboxAndroid(Profile* profile)
    : query_in_omnibox_(new QueryInOmnibox(
          AutocompleteClassifierFactory::GetForProfile(profile),
          TemplateURLServiceFactory::GetForProfile(profile))) {}

QueryInOmniboxAndroid::~QueryInOmniboxAndroid() {}

static jlong JNI_QueryInOmnibox_Init(JNIEnv* env,
                                     const JavaParamRef<jobject>& obj,
                                     const JavaParamRef<jobject>& jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  if (!profile)
    return 0;

  QueryInOmniboxAndroid* native_bridge = new QueryInOmniboxAndroid(profile);
  return reinterpret_cast<intptr_t>(native_bridge);
}

void QueryInOmniboxAndroid::Destroy(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj) {
  delete this;
}

base::android::ScopedJavaLocalRef<jstring>
QueryInOmniboxAndroid::GetDisplaySearchTerms(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint j_security_level,
    const JavaParamRef<jstring>& j_url) {
  security_state::SecurityLevel security_level =
      static_cast<security_state::SecurityLevel>(j_security_level);
  GURL url = GURL(base::android::ConvertJavaStringToUTF16(env, j_url));

  base::string16 search_terms;
  bool should_display = query_in_omnibox_->GetDisplaySearchTerms(
      security_level, url, &search_terms);

  if (!should_display)
    return nullptr;

  return base::android::ConvertUTF16ToJavaString(env, search_terms);
}

void QueryInOmniboxAndroid::SetIgnoreSecurityLevel(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    bool ignore) {
  query_in_omnibox_->set_ignore_security_level(ignore);
}
