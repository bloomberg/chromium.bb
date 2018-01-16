// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_UTIL_THREAD_TYPE_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_UTIL_THREAD_TYPE_H_

namespace blink {
namespace scheduler {

enum class ThreadType {
  kMainThread = 0,
  kUnspecifiedWorkerThread = 1,
  kCompositorThread = 2,
  kDedicatedWorkerThread = 3,
  kSharedWorkerThread = 4,
  kAnimationWorkletThread = 5,
  kServiceWorkerThread = 6,
  kAudioWorkletThread = 7,
  kCount = 8
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_UTIL_THREAD_TYPE_H_
