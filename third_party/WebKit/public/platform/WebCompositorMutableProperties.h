// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebCompositorMutableProperties_h
#define WebCompositorMutableProperties_h

namespace blink {

// TODO(vollick): we should not need a parallel enum. This must be kept in sync
// with the cc::MutableProperty enum.
enum WebCompositorMutableProperty {
    WebCompositorMutablePropertyNone = 0,
    WebCompositorMutablePropertyOpacity = 1 << 0,
    WebCompositorMutablePropertyScrollLeft = 1 << 1,
    WebCompositorMutablePropertyScrollTop = 1 << 2,
    WebCompositorMutablePropertyTransform = 1 << 3,
};

const int kNumWebCompositorMutableProperties = 4;

} // namespace blink

#endif // WebCompositorMutableProperties_h
