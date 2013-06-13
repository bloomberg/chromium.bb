// // Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_CHILD_WEBKITPLATFORMSUPPORT_CHILD_IMPL_H_
#define WEBKIT_CHILD_WEBKITPLATFORMSUPPORT_CHILD_IMPL_H_

#include "webkit/child/webkit_child_export.h"
#include "webkit/glue/webkitplatformsupport_impl.h"

namespace webkit_glue {

class FlingCurveConfiguration;

class WEBKIT_CHILD_EXPORT WebKitPlatformSupportChildImpl :
    public WebKitPlatformSupportImpl {
 public:
  WebKitPlatformSupportChildImpl();
  virtual ~WebKitPlatformSupportChildImpl();

  void SetFlingCurveParameters(
    const std::vector<float>& new_touchpad,
    const std::vector<float>& new_touchscreen);

  virtual WebKit::WebGestureCurve* createFlingAnimationCurve(
      int device_source,
      const WebKit::WebFloatPoint& velocity,
      const WebKit::WebSize& cumulative_scroll) OVERRIDE;

  scoped_ptr<FlingCurveConfiguration> fling_curve_configuration_;
};

}  // namespace webkit_glue

#endif  // WEBKIT_CHILD_WEBKITPLATFORMSUPPORT_CHILD_IMPL_H_
