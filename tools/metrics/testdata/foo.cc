// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Should not match
#ifndef FOO_OS_ANDROID_BLAT
#define FOO_OS_ANDROID_BLAT

#if defined(OS_ANDROID)

#if defined(OS_ANDROID) || defined(OS_IOS) || defined(OS_CHROMEOS)

#if !defined(OS_CAT)

#if defined(OS_WIN)

#endif  // !OS_ANDROID && !OS_IOS
#endif  // OS_CAT

#endif  // FOO_OS_ANDROID_BLAT
