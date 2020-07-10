// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PREFERENCES_PREF_CHANGE_REGISTRAR_ANDROID_H_
#define CHROME_BROWSER_ANDROID_PREFERENCES_PREF_CHANGE_REGISTRAR_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "components/prefs/pref_change_registrar.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;

class Profile;

// This class contains a PrefChangeRegistrar that observes PrefService changes
// for Android.
class PrefChangeRegistrarAndroid {
 public:
  PrefChangeRegistrarAndroid(JNIEnv* env, const JavaParamRef<jobject>& obj);
  void Destroy(JNIEnv*, const JavaParamRef<jobject>&);
  void Add(JNIEnv* env,
           const JavaParamRef<jobject>& obj,
           const jint j_pref_index);
  void Remove(JNIEnv* env,
              const JavaParamRef<jobject>& obj,
              const jint j_pref_index);

 private:
  ~PrefChangeRegistrarAndroid();
  void OnPreferenceChange(const int pref_index);

  PrefChangeRegistrar pref_change_registrar_;
  ScopedJavaGlobalRef<jobject> pref_change_registrar_jobject_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(PrefChangeRegistrarAndroid);
};

#endif  // CHROME_BROWSER_ANDROID_PREFERENCES_PREF_CHANGE_REGISTRAR_ANDROID_H_
