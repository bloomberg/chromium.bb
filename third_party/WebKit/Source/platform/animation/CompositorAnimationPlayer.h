// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorAnimationPlayer_h
#define CompositorAnimationPlayer_h

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cc/animation/animation_player.h"
#include "platform/PlatformExport.h"
#include "wtf/Noncopyable.h"

namespace blink {

class CompositorAnimation;
class WebCompositorAnimationDelegate;
class WebLayer;
class WebToCCAnimationDelegateAdapter;

// A compositor representation for AnimationPlayer.
class PLATFORM_EXPORT CompositorAnimationPlayer {
    WTF_MAKE_NONCOPYABLE(CompositorAnimationPlayer);
public:
    CompositorAnimationPlayer();
    ~CompositorAnimationPlayer();

    cc::AnimationPlayer* animationPlayer() const;

    // An animation delegate is notified when animations are started and
    // stopped. The CompositorAnimationPlayer does not take ownership of the delegate, and it is
    // the responsibility of the client to reset the layer's delegate before
    // deleting the delegate.
    void setAnimationDelegate(WebCompositorAnimationDelegate*);

    void attachLayer(WebLayer*);
    void detachLayer();
    bool isLayerAttached() const;

    void addAnimation(CompositorAnimation*);
    void removeAnimation(int animationId);
    void pauseAnimation(int animationId, double timeOffset);
    void abortAnimation(int animationId);

private:
    scoped_refptr<cc::AnimationPlayer> m_animationPlayer;
    scoped_ptr<WebToCCAnimationDelegateAdapter> m_animationDelegateAdapter;
};

} // namespace blink

#endif // CompositorAnimationPlayer_h
