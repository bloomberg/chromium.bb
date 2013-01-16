// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_SUPPORT_WEB_GESTURE_CURVE_MOCK_H_
#define WEBKIT_SUPPORT_WEB_GESTURE_CURVE_MOCK_H_

#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFloatPoint.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGestureCurve.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"

// A simple class for mocking a WebGestureCurve. The curve flings at velocity
// indefinitely.
class WebGestureCurveMock : public WebKit::WebGestureCurve {
 public:
  WebGestureCurveMock(const WebKit::WebFloatPoint& velocity,
                      const WebKit::WebSize& cumulative_scroll);
  virtual ~WebGestureCurveMock();

  // Returns false if curve has finished and can no longer be applied.
  virtual bool apply(double time,
                     WebKit::WebGestureCurveTarget* target) OVERRIDE;

 private:
  WebKit::WebFloatPoint velocity_;
  WebKit::WebSize cumulative_scroll_;

  DISALLOW_COPY_AND_ASSIGN(WebGestureCurveMock);
};

#endif  // WEBKIT_SUPPORT_WEB_GESTURE_CURVE_MOCK_H_
