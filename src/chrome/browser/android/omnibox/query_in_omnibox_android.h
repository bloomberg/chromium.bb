// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OMNIBOX_QUERY_IN_OMNIBOX_ANDROID_H_
#define CHROME_BROWSER_ANDROID_OMNIBOX_QUERY_IN_OMNIBOX_ANDROID_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"

class QueryInOmnibox;
class Profile;

// The native part of the Java QueryInOmnibox class.
class QueryInOmniboxAndroid {
 public:
  explicit QueryInOmniboxAndroid(Profile* profile);
  ~QueryInOmniboxAndroid();

  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  // Methods that forward to QueryInOmnibox:
  base::android::ScopedJavaLocalRef<jstring> GetDisplaySearchTerms(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint j_security_level,
      const base::android::JavaParamRef<jstring>& j_url);
  void SetIgnoreSecurityLevel(JNIEnv* env,
                              const base::android::JavaParamRef<jobject>& obj,
                              bool ignore);

 private:
  std::unique_ptr<QueryInOmnibox> query_in_omnibox_;

  DISALLOW_COPY_AND_ASSIGN(QueryInOmniboxAndroid);
};

#endif  // CHROME_BROWSER_ANDROID_OMNIBOX_QUERY_IN_OMNIBOX_ANDROID_H_
