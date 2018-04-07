// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_LAYOUT_AND_PAINT_ASYNC_CALLBACK_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_LAYOUT_AND_PAINT_ASYNC_CALLBACK_H_

namespace blink {

class WebLayoutAndPaintAsyncCallback {
 public:
  virtual void DidLayoutAndPaint() = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_LAYOUT_AND_PAINT_ASYNC_CALLBACK_H_
