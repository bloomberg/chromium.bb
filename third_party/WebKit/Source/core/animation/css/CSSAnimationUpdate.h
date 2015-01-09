// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSAnimationUpdate_h
#define CSSAnimationUpdate_h

#include "core/animation/Interpolation.h"
#include "wtf/HashMap.h"
#include "wtf/Vector.h"
#include "wtf/text/AtomicString.h"

namespace blink {

class AnimationPlayer;
class InertAnimation;

// This class stores the CSS Animations/Transitions information we use during a style recalc.
// This includes updates to animations/transitions as well as the Interpolations to be applied.
class CSSAnimationUpdate final : public NoBaseWillBeGarbageCollectedFinalized<CSSAnimationUpdate> {
public:
    void startAnimation(const AtomicString& animationName, PassRefPtrWillBeRawPtr<InertAnimation> animation)
    {
        animation->setName(animationName);
        NewAnimation newAnimation;
        newAnimation.name = animationName;
        newAnimation.animation = animation;
        m_newAnimations.append(newAnimation);
    }
    // Returns whether player has been suppressed and should be filtered during style application.
    bool isSuppressedAnimation(const AnimationPlayer* player) const { return m_suppressedAnimationPlayers.contains(player); }
    void cancelAnimation(const AtomicString& name, AnimationPlayer& player)
    {
        m_cancelledAnimationNames.append(name);
        m_suppressedAnimationPlayers.add(&player);
    }
    void toggleAnimationPaused(const AtomicString& name)
    {
        m_animationsWithPauseToggled.append(name);
    }
    void updateAnimationTiming(AnimationPlayer* player, PassRefPtrWillBeRawPtr<InertAnimation> animation, const Timing& timing)
    {
        UpdatedAnimationTiming updatedAnimation;
        updatedAnimation.player = player;
        updatedAnimation.animation = animation;
        updatedAnimation.newTiming = timing;
        m_animationsWithTimingUpdates.append(updatedAnimation);
        m_suppressedAnimationPlayers.add(player);
    }

    void startTransition(CSSPropertyID id, CSSPropertyID eventId, const AnimatableValue* from, const AnimatableValue* to, PassRefPtrWillBeRawPtr<InertAnimation> animation)
    {
        animation->setName(getPropertyName(id));
        NewTransition newTransition;
        newTransition.id = id;
        newTransition.eventId = eventId;
        newTransition.from = from;
        newTransition.to = to;
        newTransition.animation = animation;
        m_newTransitions.set(id, newTransition);
    }
    bool isCancelledTransition(CSSPropertyID id) const { return m_cancelledTransitions.contains(id); }
    void cancelTransition(CSSPropertyID id) { m_cancelledTransitions.add(id); }

    struct NewAnimation {
        ALLOW_ONLY_INLINE_ALLOCATION();
    public:
        void trace(Visitor* visitor)
        {
            visitor->trace(animation);
        }

        AtomicString name;
        RefPtrWillBeMember<InertAnimation> animation;
    };

    struct UpdatedAnimationTiming {
        ALLOW_ONLY_INLINE_ALLOCATION();
    public:
        void trace(Visitor* visitor)
        {
            visitor->trace(player);
            visitor->trace(animation);
        }

        RawPtrWillBeMember<AnimationPlayer> player;
        RefPtrWillBeMember<InertAnimation> animation;
        Timing newTiming;
    };

    const WillBeHeapVector<NewAnimation>& newAnimations() const { return m_newAnimations; }
    const Vector<AtomicString>& cancelledAnimationNames() const { return m_cancelledAnimationNames; }
    const WillBeHeapHashSet<RawPtrWillBeMember<const AnimationPlayer>>& suppressedAnimationAnimationPlayers() const { return m_suppressedAnimationPlayers; }
    const Vector<AtomicString>& animationsWithPauseToggled() const { return m_animationsWithPauseToggled; }
    const WillBeHeapVector<UpdatedAnimationTiming>& animationsWithTimingUpdates() const { return m_animationsWithTimingUpdates; }

    struct NewTransition {
        ALLOW_ONLY_INLINE_ALLOCATION();
    public:
        void trace(Visitor* visitor)
        {
            visitor->trace(from);
            visitor->trace(to);
            visitor->trace(animation);
        }

        CSSPropertyID id;
        CSSPropertyID eventId;
        RawPtrWillBeMember<const AnimatableValue> from;
        RawPtrWillBeMember<const AnimatableValue> to;
        RefPtrWillBeMember<InertAnimation> animation;
    };
    using NewTransitionMap = WillBeHeapHashMap<CSSPropertyID, NewTransition>;
    const NewTransitionMap& newTransitions() const { return m_newTransitions; }
    const HashSet<CSSPropertyID>& cancelledTransitions() const { return m_cancelledTransitions; }

    void adoptActiveInterpolationsForAnimations(WillBeHeapHashMap<CSSPropertyID, RefPtrWillBeMember<Interpolation>>& newMap) { newMap.swap(m_activeInterpolationsForAnimations); }
    void adoptActiveInterpolationsForTransitions(WillBeHeapHashMap<CSSPropertyID, RefPtrWillBeMember<Interpolation>>& newMap) { newMap.swap(m_activeInterpolationsForTransitions); }
    const WillBeHeapHashMap<CSSPropertyID, RefPtrWillBeMember<Interpolation>>& activeInterpolationsForAnimations() const { return m_activeInterpolationsForAnimations; }
    const WillBeHeapHashMap<CSSPropertyID, RefPtrWillBeMember<Interpolation>>& activeInterpolationsForTransitions() const { return m_activeInterpolationsForTransitions; }
    WillBeHeapHashMap<CSSPropertyID, RefPtrWillBeMember<Interpolation>>& activeInterpolationsForAnimations() { return m_activeInterpolationsForAnimations; }

    bool isEmpty() const
    {
        return m_newAnimations.isEmpty()
            && m_cancelledAnimationNames.isEmpty()
            && m_suppressedAnimationPlayers.isEmpty()
            && m_animationsWithPauseToggled.isEmpty()
            && m_animationsWithTimingUpdates.isEmpty()
            && m_newTransitions.isEmpty()
            && m_cancelledTransitions.isEmpty()
            && m_activeInterpolationsForAnimations.isEmpty()
            && m_activeInterpolationsForTransitions.isEmpty();
    }

    void trace(Visitor*);

private:
    // Order is significant since it defines the order in which new animations
    // will be started. Note that there may be multiple animations present
    // with the same name, due to the way in which we split up animations with
    // incomplete keyframes.
    WillBeHeapVector<NewAnimation> m_newAnimations;
    Vector<AtomicString> m_cancelledAnimationNames;
    WillBeHeapHashSet<RawPtrWillBeMember<const AnimationPlayer>> m_suppressedAnimationPlayers;
    Vector<AtomicString> m_animationsWithPauseToggled;
    WillBeHeapVector<UpdatedAnimationTiming> m_animationsWithTimingUpdates;

    NewTransitionMap m_newTransitions;
    HashSet<CSSPropertyID> m_cancelledTransitions;

    WillBeHeapHashMap<CSSPropertyID, RefPtrWillBeMember<Interpolation>> m_activeInterpolationsForAnimations;
    WillBeHeapHashMap<CSSPropertyID, RefPtrWillBeMember<Interpolation>> m_activeInterpolationsForTransitions;
};

} // namespace blink

WTF_ALLOW_INIT_WITH_MEM_FUNCTIONS(blink::CSSAnimationUpdate::NewAnimation);
WTF_ALLOW_INIT_WITH_MEM_FUNCTIONS(blink::CSSAnimationUpdate::UpdatedAnimationTiming);

#endif
