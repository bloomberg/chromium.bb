// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines functions that implement the static methods declared in a
// closely-related Java class in the platform-specific user interface
// implementation.  In effect, it is the entry point for all JNI calls *into*
// the C++ codebase from Java.  The separate ChromotingJniRuntime class serves
// as the corresponding exit point, and is responsible for making all JNI calls
// *out of* the C++ codebase into Java.

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "google_apis/google_api_keys.h"
#include "remoting/client/jni/chromoting_jni_instance.h"
#include "remoting/client/jni/chromoting_jni_runtime.h"

// Class and package name of the Java class that declares this file's functions.
#define JNI_IMPLEMENTATION(method) \
        Java_org_chromium_chromoting_jni_JniInterface_##method

extern "C" {

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  base::android::InitVM(vm);
  return JNI_VERSION_1_2;
}

JNIEXPORT void JNICALL JNI_IMPLEMENTATION(loadNative)(JNIEnv* env,
                                                      jobject that,
                                                      jobject context) {
  base::android::ScopedJavaLocalRef<jobject> context_activity(env, context);
  base::android::InitApplicationContext(context_activity);

  // The google_apis functions check the command-line arguments to make sure no
  // runtime API keys have been specified by the environment. Unfortunately, we
  // neither launch Chromium nor have a command line, so we need to prevent
  // them from DCHECKing out when they go looking.
  CommandLine::Init(0, NULL);

  // Create the singleton now so that the Chromoting threads will be set up.
  remoting::ChromotingJniRuntime::GetInstance();
}

JNIEXPORT jstring JNICALL JNI_IMPLEMENTATION(getApiKey)(JNIEnv* env,
                                                        jobject that) {
  return env->NewStringUTF(google_apis::GetAPIKey().c_str());
}

JNIEXPORT jstring JNICALL JNI_IMPLEMENTATION(getClientId)(JNIEnv* env,
                                                          jobject that) {
  return env->NewStringUTF(
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING).c_str());
}

JNIEXPORT jstring JNICALL JNI_IMPLEMENTATION(getClientSecret)(JNIEnv* env,
                                                              jobject that) {
  return env->NewStringUTF(
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_REMOTING).c_str());
}

JNIEXPORT void JNICALL JNI_IMPLEMENTATION(connectNative)(
    JNIEnv* env,
    jobject that,
    jstring username_jstr,
    jstring auth_token_jstr,
    jstring host_jid_jstr,
    jstring host_id_jstr,
    jstring host_pubkey_jstr,
    jstring pair_id_jstr,
    jstring pair_secret_jstr) {
  const char* username_cstr = env->GetStringUTFChars(username_jstr, NULL);
  const char* auth_token_cstr = env->GetStringUTFChars(auth_token_jstr, NULL);
  const char* host_jid_cstr = env->GetStringUTFChars(host_jid_jstr, NULL);
  const char* host_id_cstr = env->GetStringUTFChars(host_id_jstr, NULL);
  const char* host_pubkey_cstr = env->GetStringUTFChars(host_pubkey_jstr, NULL);
  const char* pair_id_cstr = env->GetStringUTFChars(pair_id_jstr, NULL);
  const char* pair_secret_cstr = env->GetStringUTFChars(pair_secret_jstr, NULL);

  remoting::ChromotingJniRuntime::GetInstance()->ConnectToHost(
      username_cstr,
      auth_token_cstr,
      host_jid_cstr,
      host_id_cstr,
      host_pubkey_cstr,
      pair_id_cstr,
      pair_secret_cstr);

  env->ReleaseStringUTFChars(username_jstr, username_cstr);
  env->ReleaseStringUTFChars(auth_token_jstr, auth_token_cstr);
  env->ReleaseStringUTFChars(host_jid_jstr, host_jid_cstr);
  env->ReleaseStringUTFChars(host_id_jstr, host_id_cstr);
  env->ReleaseStringUTFChars(host_pubkey_jstr, host_pubkey_cstr);
  env->ReleaseStringUTFChars(pair_id_jstr, pair_id_cstr);
  env->ReleaseStringUTFChars(pair_secret_jstr, pair_secret_cstr);
}

JNIEXPORT void JNICALL JNI_IMPLEMENTATION(disconnectNative)(JNIEnv* env,
                                                            jobject that) {
  remoting::ChromotingJniRuntime::GetInstance()->DisconnectFromHost();
}

JNIEXPORT void JNICALL JNI_IMPLEMENTATION(authenticationResponse)(
    JNIEnv* env,
    jobject that,
    jstring pin_jstr,
    jboolean create_pair) {
  const char* pin_cstr = env->GetStringUTFChars(pin_jstr, NULL);

  remoting::ChromotingJniRuntime::GetInstance()->
      session()->ProvideSecret(pin_cstr, create_pair);

  env->ReleaseStringUTFChars(pin_jstr, pin_cstr);
}

JNIEXPORT void JNICALL JNI_IMPLEMENTATION(scheduleRedrawNative)(
    JNIEnv* env,
    jobject that) {
  remoting::ChromotingJniRuntime::GetInstance()->session()->RedrawDesktop();
}

JNIEXPORT void JNICALL JNI_IMPLEMENTATION(mouseActionNative)(
    JNIEnv* env,
    jobject that,
    jint x,
    jint y,
    jint which_button,
    jboolean button_down) {
  // Button must be within the bounds of the MouseEvent_MouseButton enum.
  DCHECK(which_button >= 0 && which_button < 5);

  remoting::ChromotingJniRuntime::GetInstance()->session()->PerformMouseAction(
      x,
      y,
      static_cast<remoting::protocol::MouseEvent_MouseButton>(which_button),
      button_down);
}

JNIEXPORT void JNICALL JNI_IMPLEMENTATION(keyboardActionNative)(
    JNIEnv* env,
    jobject that,
    jint key_code,
    jboolean key_down) {
  remoting::ChromotingJniRuntime::GetInstance()->session()->
      PerformKeyboardAction(key_code, key_down);
}

}  // extern "C"
