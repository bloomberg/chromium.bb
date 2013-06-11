// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_LIBCORKSCREW_OVERRIDES_CUTILS_LOG_H_
#define THIRD_PARTY_LIBCORKSCREW_OVERRIDES_CUTILS_LOG_H_

#include <android/log.h>

#define ALOGV(...) \
    __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#endif  // THIRD_PARTY_LIBCORKSCREW_OVERRIDES_CUTILS_LOG_H_
