// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebFrameOwnerProperties_h
#define WebFrameOwnerProperties_h

#include "../platform/WebFeaturePolicy.h"
#include "../platform/WebString.h"
#include "../platform/WebVector.h"
#include "third_party/WebKit/public/platform/modules/permissions/permission.mojom-shared.h"

#include <algorithm>

namespace blink {

struct WebFrameOwnerProperties {
  enum class ScrollingMode { Auto, AlwaysOff, AlwaysOn, Last = AlwaysOn };

  WebString name;  // browsing context container's name
  ScrollingMode scrollingMode;
  int marginWidth;
  int marginHeight;
  bool allowFullscreen;
  bool allowPaymentRequest;
  WebString requiredCsp;
  WebVector<mojom::PermissionName> delegatedPermissions;

 public:
  WebVector<WebFeaturePolicyFeature> allowedFeatures;

  WebFrameOwnerProperties()
      : scrollingMode(ScrollingMode::Auto),
        marginWidth(-1),
        marginHeight(-1),
        allowFullscreen(false),
        allowPaymentRequest(false) {}

#if INSIDE_BLINK
  WebFrameOwnerProperties(
      const WebString& name,
      ScrollbarMode scrollingMode,
      int marginWidth,
      int marginHeight,
      bool allowFullscreen,
      bool allowPaymentRequest,
      const WebString& requiredCsp,
      const WebVector<mojom::PermissionName>& delegatedPermissions,
      const WebVector<WebFeaturePolicyFeature>& allowedFeatures)
      : name(name),
        scrollingMode(static_cast<ScrollingMode>(scrollingMode)),
        marginWidth(marginWidth),
        marginHeight(marginHeight),
        allowFullscreen(allowFullscreen),
        allowPaymentRequest(allowPaymentRequest),
        requiredCsp(requiredCsp),
        delegatedPermissions(delegatedPermissions),
        allowedFeatures(allowedFeatures) {}
#endif
};

}  // namespace blink

#endif
