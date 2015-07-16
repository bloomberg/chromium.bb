// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/page/scrolling/ScrollStateCallback.h"

namespace blink {

NativeScrollBehavior ScrollStateCallback::toNativeScrollBehavior(String nativeScrollBehavior)
{
    DEFINE_STATIC_LOCAL(const String, disable, ("disable-native-scroll"));
    DEFINE_STATIC_LOCAL(const String, before, ("perform-before-native-scroll"));
    DEFINE_STATIC_LOCAL(const String, after, ("perform-after-native-scroll"));

    if (nativeScrollBehavior == disable)
        return NativeScrollBehavior::DisableNativeScroll;
    if (nativeScrollBehavior == before)
        return NativeScrollBehavior::PerformBeforeNativeScroll;
    if (nativeScrollBehavior == after)
        return NativeScrollBehavior::PerformAfterNativeScroll;

    ASSERT_NOT_REACHED();
    return NativeScrollBehavior::DisableNativeScroll;
}

} // namespace blink
