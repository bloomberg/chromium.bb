// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSAnimationUpdate_h
#define CSSAnimationUpdate_h

#include "core/animation/Interpolation.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/animation/css/CSSAnimatableValueFactory.h"
#include "core/animation/css/CSSPropertyEquality.h"
#include "core/css/CSSKeyframesRule.h"
#include "core/layout/LayoutObject.h"
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
    class NewAnimation {
        ALLOW_ONLY_INLINE_ALLOCATION();
    public:
        NewAnimation()
            : styleRuleVersion(0)
        {
        }

        NewAnimation(AtomicString name, PassRefPtrWillBeRawPtr<InertAnimation> animation, Timing timing, PassRefPtrWillBeRawPtr<StyleRuleKeyframes> styleRule)
            : name(name)
            , animation(animation)
            , timing(timing)
            , styleRule(styleRule)
            , styleRuleVersion(this->styleRule->version())
        {
        }

        DEFINE_INLINE_TRACE()
        {
            visitor->trace(animation);
            visitor->trace(styleRule);
        }

        AtomicString name;
        RefPtrWillBeMember<InertAnimation> animation;
        Timing timing;
        RefPtrWillBeMember<StyleRuleKeyframes> styleRule;
        unsigned styleRuleVersion;
    };

    class UpdatedAnimation {
        ALLOW_ONLY_INLINE_ALLOCATION();
    public:
        UpdatedAnimation()
            : styleRuleVersion(0)
        {
        }

        UpdatedAnimation(AtomicString name, AnimationPlayer* player, PassRefPtrWillBeRawPtr<InertAnimation> animation, Timing specifiedTiming, PassRefPtrWillBeRawPtr<StyleRuleKeyframes> styleRule)
            : name(name)
            , player(player)
            , animation(animation)
            , specifiedTiming(specifiedTiming)
            , styleRule(styleRule)
            , styleRuleVersion(this->styleRule->version())
        {
        }

        DEFINE_INLINE_TRACE()
        {
            visitor->trace(player);
            visitor->trace(animation);
            visitor->trace(styleRule);
        }

        AtomicString name;
        RawPtrWillBeMember<AnimationPlayer> player;
        RefPtrWillBeMember<InertAnimation> animation;
        Timing specifiedTiming;
        RefPtrWillBeMember<StyleRuleKeyframes> styleRule;
        unsigned styleRuleVersion;
    };

    class UpdatedAnimationStyle {
        ALLOW_ONLY_INLINE_ALLOCATION();
    public:
        struct CompositableStyleSnapshot {
            DISALLOW_ALLOCATION();

        public:
            RefPtrWillBeMember<AnimatableValue> opacity;
            RefPtrWillBeMember<AnimatableValue> transform;
            RefPtrWillBeMember<AnimatableValue> webkitFilter;

            DEFINE_INLINE_TRACE()
            {
                visitor->trace(opacity);
                visitor->trace(transform);
                visitor->trace(webkitFilter);
            }
        };

        UpdatedAnimationStyle()
        {
        }

        UpdatedAnimationStyle(AnimationPlayer* player, KeyframeEffectModelBase* effect, const UpdatedAnimationStyle::CompositableStyleSnapshot& snapshot)
            : player(player)
            , effect(effect)
            , snapshot(snapshot)
        {
        }

        DEFINE_INLINE_TRACE()
        {
            visitor->trace(player);
            visitor->trace(effect);
            visitor->trace(snapshot);
        }

        RawPtrWillBeMember<AnimationPlayer> player;
        RawPtrWillBeMember<KeyframeEffectModelBase> effect;
        CompositableStyleSnapshot snapshot;
    };

    void startAnimation(const AtomicString& animationName, PassRefPtrWillBeRawPtr<InertAnimation> animation, const Timing& timing, PassRefPtrWillBeRawPtr<StyleRuleKeyframes> styleRule)
    {
        animation->setName(animationName);
        m_newAnimations.append(NewAnimation(animationName, animation, timing, styleRule));
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
    void updateAnimation(const AtomicString& name, AnimationPlayer* player, PassRefPtrWillBeRawPtr<InertAnimation> animation, const Timing& specifiedTiming,
        PassRefPtrWillBeRawPtr<StyleRuleKeyframes> styleRule)
    {
        m_animationsWithUpdates.append(UpdatedAnimation(name, player, animation, specifiedTiming, styleRule));
        m_suppressedAnimationPlayers.add(player);
    }
    void updateAnimationStyle(AnimationPlayer* player, KeyframeEffectModelBase* effect, LayoutObject* renderer, const LayoutStyle& newStyle)
    {
        UpdatedAnimationStyle::CompositableStyleSnapshot snapshot;
        if (renderer) {
            const LayoutStyle& oldStyle = renderer->styleRef();
            if (!CSSPropertyEquality::propertiesEqual(CSSPropertyOpacity, oldStyle, newStyle) && effect->affects(CSSPropertyOpacity))
                snapshot.opacity = CSSAnimatableValueFactory::create(CSSPropertyOpacity, newStyle);
            if (!CSSPropertyEquality::propertiesEqual(CSSPropertyTransform, oldStyle, newStyle) && effect->affects(CSSPropertyTransform))
                snapshot.transform = CSSAnimatableValueFactory::create(CSSPropertyTransform, newStyle);
            if (!CSSPropertyEquality::propertiesEqual(CSSPropertyWebkitFilter, oldStyle, newStyle) && effect->affects(CSSPropertyWebkitFilter))
                snapshot.webkitFilter = CSSAnimatableValueFactory::create(CSSPropertyWebkitFilter, newStyle);
        }

        m_animationsWithStyleUpdates.append(UpdatedAnimationStyle(player, effect, snapshot));
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
    void finishTransition(CSSPropertyID id) { m_finishedTransitions.add(id); }

    const WillBeHeapVector<NewAnimation>& newAnimations() const { return m_newAnimations; }
    const Vector<AtomicString>& cancelledAnimationNames() const { return m_cancelledAnimationNames; }
    const WillBeHeapHashSet<RawPtrWillBeMember<const AnimationPlayer>>& suppressedAnimationAnimationPlayers() const { return m_suppressedAnimationPlayers; }
    const Vector<AtomicString>& animationsWithPauseToggled() const { return m_animationsWithPauseToggled; }
    const WillBeHeapVector<UpdatedAnimation>& animationsWithUpdates() const { return m_animationsWithUpdates; }
    const WillBeHeapVector<UpdatedAnimationStyle>& animationsWithStyleUpdates() const { return m_animationsWithStyleUpdates; }

    struct NewTransition {
        ALLOW_ONLY_INLINE_ALLOCATION();
    public:
        DEFINE_INLINE_TRACE()
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
    const HashSet<CSSPropertyID>& finishedTransitions() const { return m_finishedTransitions; }

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
            && m_animationsWithUpdates.isEmpty()
            && m_animationsWithStyleUpdates.isEmpty()
            && m_newTransitions.isEmpty()
            && m_cancelledTransitions.isEmpty()
            && m_finishedTransitions.isEmpty()
            && m_activeInterpolationsForAnimations.isEmpty()
            && m_activeInterpolationsForTransitions.isEmpty();
    }

    DECLARE_TRACE();

private:
    // Order is significant since it defines the order in which new animations
    // will be started. Note that there may be multiple animations present
    // with the same name, due to the way in which we split up animations with
    // incomplete keyframes.
    WillBeHeapVector<NewAnimation> m_newAnimations;
    Vector<AtomicString> m_cancelledAnimationNames;
    WillBeHeapHashSet<RawPtrWillBeMember<const AnimationPlayer>> m_suppressedAnimationPlayers;
    Vector<AtomicString> m_animationsWithPauseToggled;
    WillBeHeapVector<UpdatedAnimation> m_animationsWithUpdates;
    WillBeHeapVector<UpdatedAnimationStyle> m_animationsWithStyleUpdates;

    NewTransitionMap m_newTransitions;
    HashSet<CSSPropertyID> m_cancelledTransitions;
    HashSet<CSSPropertyID> m_finishedTransitions;

    WillBeHeapHashMap<CSSPropertyID, RefPtrWillBeMember<Interpolation>> m_activeInterpolationsForAnimations;
    WillBeHeapHashMap<CSSPropertyID, RefPtrWillBeMember<Interpolation>> m_activeInterpolationsForTransitions;
};

} // namespace blink

WTF_ALLOW_INIT_WITH_MEM_FUNCTIONS(blink::CSSAnimationUpdate::NewAnimation);
WTF_ALLOW_INIT_WITH_MEM_FUNCTIONS(blink::CSSAnimationUpdate::UpdatedAnimation);

#endif
