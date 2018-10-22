// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_CREDENTIAL_ANDROID_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_CREDENTIAL_ANDROID_H_

#include <stddef.h>

#include "base/android/jni_array.h"
#include "base/android/scoped_java_ref.h"
#include "components/autofill/core/common/password_form.h"

// Creates Java counterpart of autofill::PasswordForm, assigning it a |position|
// in case form is part of some array of forms and |type| which should be either
// local or federated.
base::android::ScopedJavaLocalRef<jobject> CreateNativeCredential(
    JNIEnv* env,
    const autofill::PasswordForm& password_form,
    int position);

// Creates Java Credential[] array of size |size|.
base::android::ScopedJavaLocalRef<jobjectArray> CreateNativeCredentialArray(
    JNIEnv* env,
    size_t size);

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_CREDENTIAL_ANDROID_H_
