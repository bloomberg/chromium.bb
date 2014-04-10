// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SampledEffect_h
#define SampledEffect_h

#include "core/animation/Animation.h"
#include "core/animation/AnimationPlayer.h"
#include "core/animation/Interpolation.h"
#include "wtf/BitArray.h"
#include "wtf/Vector.h"

namespace WebCore {

class SampledEffect {
public:
    static PassOwnPtr<SampledEffect> create(Animation* animation, PassOwnPtrWillBeRawPtr<WillBeHeapVector<RefPtrWillBeMember<Interpolation> > > interpolations)
    {
        return adoptPtr(new SampledEffect(animation, interpolations));
    }

    bool canChange() const;
    void clear();

    const WillBeHeapVector<RefPtrWillBeMember<Interpolation> >& interpolations() const { return *m_interpolations; }
    void setInterpolations(PassOwnPtrWillBeRawPtr<WillBeHeapVector<RefPtrWillBeMember<Interpolation> > > interpolations) { m_interpolations = interpolations; }

    Animation* animation() const { return m_animation; }
    const AnimationPlayer::SortInfo& sortInfo() const { return m_playerSortInfo; }
    Animation::Priority priority() const { return m_priority; }

    void removeReplacedInterpolationsIfNeeded(const BitArray<numCSSProperties>&);

private:
    SampledEffect(Animation*, PassOwnPtrWillBeRawPtr<WillBeHeapVector<RefPtrWillBeMember<Interpolation> > >);

    // When Animation and AnimationPlayer are moved to Oilpan, we won't need a
    // handle on the player and should only keep a weak pointer to the animation.
    RefPtr<AnimationPlayer> m_player;
    Animation* m_animation;
    OwnPtrWillBePersistent<WillBeHeapVector<RefPtrWillBeMember<Interpolation> > > m_interpolations;
    AnimationPlayer::SortInfo m_playerSortInfo;
    Animation::Priority m_priority;
};

} // namespace WebCore

#endif
