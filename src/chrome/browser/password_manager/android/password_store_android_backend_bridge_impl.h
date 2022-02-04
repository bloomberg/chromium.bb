// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_BRIDGE_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_BRIDGE_IMPL_H_

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_bridge.h"
#include "components/password_manager/core/browser/password_store_backend.h"

// Native side of the JNI bridge to forward password store requests to Java.
// JNI code is expensive to test. Therefore, any logic beyond data conversion
// should either live in `PasswordStoreAndroidBackend` or a component that is
// used by the java-side of this bridge.
class PasswordStoreAndroidBackendBridgeImpl
    : public password_manager::PasswordStoreAndroidBackendBridge {
 public:
  PasswordStoreAndroidBackendBridgeImpl();
  PasswordStoreAndroidBackendBridgeImpl(
      PasswordStoreAndroidBackendBridgeImpl&&) = delete;
  PasswordStoreAndroidBackendBridgeImpl(
      const PasswordStoreAndroidBackendBridgeImpl&) = delete;
  PasswordStoreAndroidBackendBridgeImpl& operator=(
      PasswordStoreAndroidBackendBridgeImpl&&) = delete;
  PasswordStoreAndroidBackendBridgeImpl& operator=(
      const PasswordStoreAndroidBackendBridgeImpl&) = delete;
  ~PasswordStoreAndroidBackendBridgeImpl() override;

  // Called via JNI. Called when the api call with `job_id` finished and
  // provides the resulting `passwords`.
  void OnCompleteWithLogins(
      JNIEnv* env,
      jint job_id,
      const base::android::JavaParamRef<jbyteArray>& passwords);

  // Called via JNI. Called when the api call with `job_id` finished and
  // provides serialized PasswordWithLocalData identifying the added 'login'.
  void OnLoginAdded(JNIEnv* env,
                    jint job_id,
                    const base::android::JavaParamRef<jbyteArray>& login);

  // Called via JNI. Called when the api call with `job_id` finished and
  // provides serialized PasswordWithLocalData identifying the updated 'login'.
  void OnLoginUpdated(JNIEnv* env,
                      jint job_id,
                      const base::android::JavaParamRef<jbyteArray>& login);

  // Called via JNI. Called when the api call with `job_id` finished and
  // provides Serialized PasswordSpecificsData identifying the deleted 'login'.
  void OnLoginDeleted(JNIEnv* env,
                      jint job_id,
                      const base::android::JavaParamRef<jbyteArray>& login);

  // Called via JNI. Called when the api call with `job_id` finished with
  // an exception.
  void OnError(JNIEnv* env, jint job_id, jint error_type, jint api_error_code);

 private:
  // Implements PasswordStoreAndroidBackendBridge interface.
  void SetConsumer(base::WeakPtr<Consumer> consumer) override;
  [[nodiscard]] JobId GetAllLogins(
      password_manager::PasswordStoreOperationTarget target) override;
  [[nodiscard]] JobId GetAutofillableLogins() override;
  [[nodiscard]] JobId GetLoginsForSignonRealm(
      const std::string& signon_realm) override;
  [[nodiscard]] JobId AddLogin(
      const password_manager::PasswordForm& form) override;
  [[nodiscard]] JobId UpdateLogin(
      const password_manager::PasswordForm& form) override;
  [[nodiscard]] JobId RemoveLogin(
      const password_manager::PasswordForm& form,
      password_manager::PasswordStoreOperationTarget target) override;

  [[nodiscard]] JobId GetNextJobId();

  // This member stores the unique ID last used for an API request.
  JobId last_job_id_{0};

  // Weak reference to the `Consumer` that is notified when a job completes. It
  // outlives this bridge but tasks may be posted to it.
  base::WeakPtr<Consumer> consumer_ = nullptr;

  // This object is an instance of PasswordStoreAndroidBackendBridgeImpl, i.e.
  // the Java counterpart to this class.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_BRIDGE_IMPL_H_
