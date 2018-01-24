// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebComputedAXTree_h
#define WebComputedAXTree_h

#include "third_party/WebKit/public/platform/WebString.h"

namespace blink {

class WebComputedAXTree {
 public:
  virtual ~WebComputedAXTree() {}

  virtual bool ComputeAccessibilityTree() = 0;
  virtual WebString GetNameForAXNode(int32_t axID) = 0;
  virtual WebString GetRoleForAXNode(int32_t axID) = 0;
};

}  // namespace blink

#endif  // WebComputedAXTree_h
