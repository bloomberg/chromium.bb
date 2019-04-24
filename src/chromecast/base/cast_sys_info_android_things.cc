// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/cast_sys_info_android_things.h"

#include "base/android/jni_string.h"
#include "chromecast/chromecast_buildflags.h"
#if BUILDFLAG(IS_ANDROID_THINGS_NON_PUBLIC)
#include "base/android/jni_array.h"
#include "jni/CastSysInfoAndroidThings_jni.h"
#endif

namespace chromecast {

CastSysInfoAndroidThings::CastSysInfoAndroidThings() = default;
CastSysInfoAndroidThings::~CastSysInfoAndroidThings() = default;

std::string CastSysInfoAndroidThings::GetProductName() {
#if BUILDFLAG(IS_ANDROID_THINGS_NON_PUBLIC)
  JNIEnv* env = base::android::AttachCurrentThread();
  return base::android::ConvertJavaStringToUTF8(
      Java_CastSysInfoAndroidThings_getProductName(env));
#else
  return "";
#endif
}

std::string CastSysInfoAndroidThings::GetDeviceModel() {
#if BUILDFLAG(IS_ANDROID_THINGS_NON_PUBLIC)
  JNIEnv* env = base::android::AttachCurrentThread();
  return base::android::ConvertJavaStringToUTF8(
      Java_CastSysInfoAndroidThings_getDeviceModel(env));
#else
  return "";
#endif
}

std::string CastSysInfoAndroidThings::GetManufacturer() {
#if BUILDFLAG(IS_ANDROID_THINGS_NON_PUBLIC)
  JNIEnv* env = base::android::AttachCurrentThread();
  return base::android::ConvertJavaStringToUTF8(
      Java_CastSysInfoAndroidThings_getManufacturer(env));
#else
  return "";
#endif
}

std::string CastSysInfoAndroidThings::GetSystemReleaseChannel() {
#if BUILDFLAG(IS_ANDROID_THINGS_NON_PUBLIC)
  JNIEnv* env = base::android::AttachCurrentThread();
  return base::android::ConvertJavaStringToUTF8(
      Java_CastSysInfoAndroidThings_getReleaseChannel(env));
#else
  return "";
#endif
}

// static
std::vector<std::string> CastSysInfo::GetFactoryLocaleList() {
  std::vector<std::string> locale_list;
#if BUILDFLAG(IS_ANDROID_THINGS_NON_PUBLIC)
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::AppendJavaStringArrayToStringVector(
      env, Java_CastSysInfoAndroidThings_getFactoryLocaleList(env),
      &locale_list);
#endif
  if (locale_list.empty()) {
    CastSysInfoAndroidThings sys_info;
    std::string factory_locale2;
    std::string factory_locale1 = sys_info.GetFactoryLocale(&factory_locale2);
    locale_list = {std::move(factory_locale1)};
    if (!factory_locale2.empty()) {
      locale_list.push_back(factory_locale2);
    }
  }
  return locale_list;
}

}  // namespace chromecast
