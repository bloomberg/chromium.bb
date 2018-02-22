// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorAnimationClient_h
#define CompositorAnimationClient_h

#include "public/platform/WebCommon.h"

namespace blink {

class CompositorAnimation;

// A client for compositor representation of Animation.
class BLINK_PLATFORM_EXPORT CompositorAnimationClient {
 public:
  virtual ~CompositorAnimationClient();

  virtual CompositorAnimation* GetCompositorAnimation() const = 0;
};

}  // namespace blink

#endif  // CompositorAnimationClient_h
