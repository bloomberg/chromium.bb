// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HTMLImageFallbackHelper_h
#define HTMLImageFallbackHelper_h

#include "platform/wtf/Allocator.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

class Element;
class ComputedStyle;

class HTMLImageFallbackHelper {
  STATIC_ONLY(HTMLImageFallbackHelper);

 public:
  static void CreateAltTextShadowTree(Element&);
  static scoped_refptr<ComputedStyle> CustomStyleForAltText(
      Element&,
      scoped_refptr<ComputedStyle> new_style);
};

}  // namespace blink

#endif  // HTMLImageFallbackHelper_h
