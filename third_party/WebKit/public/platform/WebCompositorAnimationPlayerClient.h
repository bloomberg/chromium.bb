// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebCompositorAnimationPlayerClient_h
#define WebCompositorAnimationPlayerClient_h

namespace blink {

class WebCompositorAnimationPlayer;

// A client for compositor representation of AnimationPlayer.
class WebCompositorAnimationPlayerClient {
public:
    virtual ~WebCompositorAnimationPlayerClient() { }

    virtual WebCompositorAnimationPlayer* compositorPlayer() const = 0;
};

} // namespace blink

#endif // WebCompositorAnimationPlayerClient_h
