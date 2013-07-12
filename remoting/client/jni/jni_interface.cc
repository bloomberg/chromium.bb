// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines functions that implement the static methods declared in a
// closely-related Java class in the platform-specific user interface
// implementation.  In effect, it is the entry point for all JNI calls *into*
// the C++ codebase from Java.  The separate ChromotingJNIInstance class serves
// as the corresponding exit point, and is responsible for making all JNI calls
// *out of* the C++ codebase into Java.

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "google_apis/google_api_keys.h"
#include "remoting/client/jni/chromoting_jni_instance.h"

// Class and package name of the Java class that declares this file's functions.
#define JNI_IMPLEMENTATION(method) \
        Java_org_chromium_chromoting_jni_JNIInterface_##method

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

  remoting::ChromotingJNIInstance::GetInstance(); // Initialize threads now.
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

JNIEXPORT void JNICALL JNI_IMPLEMENTATION(connectNative)(JNIEnv* env,
                                                         jobject that,
                                                         jstring username,
                                                         jstring auth_token,
                                                         jstring host_jid,
                                                         jstring host_id,
                                                         jstring host_pubkey) {
  remoting::ChromotingJNIInstance::GetInstance()->ConnectToHost(username,
                                                                auth_token,
                                                                host_jid,
                                                                host_id,
                                                                host_pubkey);
}

JNIEXPORT void JNICALL JNI_IMPLEMENTATION(disconnectNative)(JNIEnv* env,
                                                            jobject that) {
  remoting::ChromotingJNIInstance::GetInstance()->DisconnectFromHost();
}

JNIEXPORT void JNICALL JNI_IMPLEMENTATION(authenticationResponse)(JNIEnv* env,
                                                                  jobject that,
                                                                  jstring pin) {
  remoting::ChromotingJNIInstance::GetInstance()->AuthenticateWithPin(pin);
}

}  // extern "C"
