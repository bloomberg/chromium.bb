/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/animation/css/CSSAnimations.h"

#include "StylePropertyShorthand.h"
#include "core/animation/ActiveAnimations.h"
#include "core/animation/DocumentTimeline.h"
#include "core/animation/KeyframeAnimationEffect.h"
#include "core/animation/css/CSSAnimatableValueFactory.h"
#include "core/css/CSSKeyframeRule.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Element.h"
#include "core/events/ThreadLocalEventNames.h"
#include "core/events/TransitionEvent.h"
#include "core/events/WebKitAnimationEvent.h"
#include "core/platform/animation/CSSAnimationDataList.h"
#include "core/platform/animation/TimingFunction.h"
#include "wtf/HashSet.h"

namespace WebCore {

struct CandidateTransition {
    CandidateTransition(PassRefPtr<AnimatableValue> from, PassRefPtr<AnimatableValue> to, const CSSAnimationData* anim)
        : from(from)
        , to(to)
        , anim(anim)
    {
    }
    CandidateTransition() { } // The HashMap calls the default ctor
    RefPtr<AnimatableValue> from;
    RefPtr<AnimatableValue> to;
    const CSSAnimationData* anim;
};
typedef HashMap<CSSPropertyID, CandidateTransition> CandidateTransitionMap;

namespace {

bool isEarlierPhase(TimedItem::Phase target, TimedItem::Phase reference)
{
    ASSERT(target != TimedItem::PhaseNone);
    ASSERT(reference != TimedItem::PhaseNone);
    return target < reference;
}

bool isLaterPhase(TimedItem::Phase target, TimedItem::Phase reference)
{
    ASSERT(target != TimedItem::PhaseNone);
    ASSERT(reference != TimedItem::PhaseNone);
    return target > reference;
}

// Returns the default timing function.
const PassRefPtr<TimingFunction> timingFromAnimationData(const CSSAnimationData* animationData, Timing& timing)
{
    if (animationData->isDelaySet())
        timing.startDelay = animationData->delay();
    if (animationData->isDurationSet()) {
        timing.iterationDuration = animationData->duration();
        timing.hasIterationDuration = true;
    }
    if (animationData->isIterationCountSet()) {
        if (animationData->iterationCount() == CSSAnimationData::IterationCountInfinite)
            timing.iterationCount = std::numeric_limits<double>::infinity();
        else
            timing.iterationCount = animationData->iterationCount();
    }
    if (animationData->isFillModeSet()) {
        switch (animationData->fillMode()) {
        case AnimationFillModeForwards:
            timing.fillMode = Timing::FillModeForwards;
            break;
        case AnimationFillModeBackwards:
            timing.fillMode = Timing::FillModeBackwards;
            break;
        case AnimationFillModeBoth:
            timing.fillMode = Timing::FillModeBoth;
            break;
        case AnimationFillModeNone:
            timing.fillMode = Timing::FillModeNone;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    } else {
        timing.fillMode = Timing::FillModeNone;
    }
    if (animationData->isDirectionSet()) {
        switch (animationData->direction()) {
        case CSSAnimationData::AnimationDirectionNormal:
            timing.direction = Timing::PlaybackDirectionNormal;
            break;
        case CSSAnimationData::AnimationDirectionAlternate:
            timing.direction = Timing::PlaybackDirectionAlternate;
            break;
        case CSSAnimationData::AnimationDirectionReverse:
            timing.direction = Timing::PlaybackDirectionReverse;
            break;
        case CSSAnimationData::AnimationDirectionAlternateReverse:
            timing.direction = Timing::PlaybackDirectionAlternateReverse;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    }
    return animationData->isTimingFunctionSet() ? animationData->timingFunction() : CSSAnimationData::initialAnimationTimingFunction();
}

void calculateCandidateTransitionForProperty(const CSSAnimationData* anim, CSSPropertyID id, const RenderStyle* oldStyle, const RenderStyle* newStyle, CandidateTransitionMap& candidateMap)
{
    RefPtr<AnimatableValue> from = CSSAnimatableValueFactory::create(id, oldStyle);
    RefPtr<AnimatableValue> to = CSSAnimatableValueFactory::create(id, newStyle);
    // If we have multiple transitions on the same property, we will use the
    // last one since we iterate over them in order and this will override
    // a previously set CandidateTransition.
    if (!from->equals(to.get()))
        candidateMap.add(id, CandidateTransition(from.release(), to.release(), anim));
}

void computeCandidateTransitions(const RenderStyle* oldStyle, const RenderStyle* newStyle, CandidateTransitionMap& candidateMap, HashSet<CSSPropertyID>& listedProperties)
{
    if (!newStyle->transitions())
        return;

    for (size_t i = 0; i < newStyle->transitions()->size(); ++i) {
        const CSSAnimationData* anim = newStyle->transitions()->animation(i);
        CSSAnimationData::AnimationMode mode = anim->animationMode();
        if (anim->duration() + anim->delay() <= 0 || mode == CSSAnimationData::AnimateNone)
            continue;

        bool animateAll = mode == CSSAnimationData::AnimateAll;
        ASSERT(animateAll || mode == CSSAnimationData::AnimateSingleProperty);
        const StylePropertyShorthand& propertyList = animateAll ? CSSAnimations::animatableProperties() : shorthandForProperty(anim->property());
        if (!propertyList.length()) {
            listedProperties.add(anim->property());
            calculateCandidateTransitionForProperty(anim, anim->property(), oldStyle, newStyle, candidateMap);
        } else {
            for (unsigned i = 0; i < propertyList.length(); ++i) {
                CSSPropertyID id = propertyList.properties()[i];
                if (!animateAll && !CSSAnimations::isAnimatableProperty(id))
                    continue;
                listedProperties.add(id);
                calculateCandidateTransitionForProperty(anim, id, oldStyle, newStyle, candidateMap);
            }
        }
    }
}

} // namespace

CSSAnimationUpdateScope::CSSAnimationUpdateScope(Element* target)
    : m_target(target)
{
    if (!m_target)
        return;
    ActiveAnimations* activeAnimations = m_target->activeAnimations();
    CSSAnimations* cssAnimations = activeAnimations ? activeAnimations->cssAnimations() : 0;
    // It's possible than an update was created outside an update scope. That's harmless
    // but we must clear it now to avoid applying it if an updated replacement is not
    // created in this scope.
    if (cssAnimations)
        cssAnimations->setPendingUpdate(nullptr);
}

CSSAnimationUpdateScope::~CSSAnimationUpdateScope()
{
    if (!m_target)
        return;
    ActiveAnimations* activeAnimations = m_target->activeAnimations();
    CSSAnimations* cssAnimations = activeAnimations ? activeAnimations->cssAnimations() : 0;
    if (cssAnimations)
        cssAnimations->maybeApplyPendingUpdate(m_target);
}

PassOwnPtr<CSSAnimationUpdate> CSSAnimations::calculateUpdate(Element* element, const RenderStyle* style, StyleResolver* resolver)
{
    ASSERT(RuntimeEnabledFeatures::webAnimationsCSSEnabled());
    OwnPtr<CSSAnimationUpdate> update = adoptPtr(new CSSAnimationUpdate());
    calculateAnimationUpdate(update.get(), element, style, resolver);
    calculateTransitionUpdate(update.get(), element, style);
    return update->isEmpty() ? nullptr : update.release();
}

void CSSAnimations::calculateAnimationUpdate(CSSAnimationUpdate* update, Element* element, const RenderStyle* style, StyleResolver* resolver)
{
    ActiveAnimations* activeAnimations = element->activeAnimations();
    const CSSAnimationDataList* animationDataList = style->animations();
    const CSSAnimations* cssAnimations = activeAnimations ? activeAnimations->cssAnimations() : 0;

    HashSet<AtomicString> inactive;
    if (cssAnimations)
        for (AnimationMap::const_iterator iter = cssAnimations->m_animations.begin(); iter != cssAnimations->m_animations.end(); ++iter)
            inactive.add(iter->key);

    if (style->display() != NONE) {
        for (size_t i = 0; animationDataList && i < animationDataList->size(); ++i) {
            const CSSAnimationData* animationData = animationDataList->animation(i);
            if (animationData->isNoneAnimation())
                continue;
            ASSERT(animationData->isValidAnimation());
            AtomicString animationName(animationData->name());

            if (cssAnimations) {
                AnimationMap::const_iterator existing(cssAnimations->m_animations.find(animationName));
                if (existing != cssAnimations->m_animations.end()) {
                    // FIXME: The play-state of this animation might have changed, record the change in the update.
                    inactive.remove(animationName);
                    continue;
                }
            }

            Timing timing;
            RefPtr<TimingFunction> defaultTimingFunction = timingFromAnimationData(animationData, timing);
            Vector<std::pair<KeyframeAnimationEffect::KeyframeVector, RefPtr<TimingFunction> > > keyframesAndTimingFunctions;
            resolver->resolveKeyframes(element, style, animationName, defaultTimingFunction.get(), keyframesAndTimingFunctions);
            if (!keyframesAndTimingFunctions.isEmpty()) {
                HashSet<RefPtr<InertAnimation> > animations;
                for (size_t j = 0; j < keyframesAndTimingFunctions.size(); ++j) {
                    ASSERT(!keyframesAndTimingFunctions[j].first.isEmpty());
                    timing.timingFunction = keyframesAndTimingFunctions[j].second;
                    // FIXME: crbug.com/268791 - Keyframes are already normalized, perhaps there should be a flag on KeyframeAnimationEffect to skip normalization.
                    animations.add(InertAnimation::create(KeyframeAnimationEffect::create(keyframesAndTimingFunctions[j].first), timing));
                }
                update->startAnimation(animationName, animations);
            }
        }
    }

    for (HashSet<AtomicString>::const_iterator iter = inactive.begin(); iter != inactive.end(); ++iter)
        update->cancelAnimation(*iter, cssAnimations->m_animations.get(*iter));
}

void CSSAnimations::maybeApplyPendingUpdate(Element* element)
{
    if (!element->renderer())
        m_pendingUpdate = nullptr;

    if (!m_pendingUpdate)
        return;

    OwnPtr<CSSAnimationUpdate> update = m_pendingUpdate.release();

    for (Vector<AtomicString>::const_iterator iter = update->cancelledAnimationNames().begin(); iter != update->cancelledAnimationNames().end(); ++iter) {
        const HashSet<RefPtr<Player> >& players = m_animations.take(*iter);
        for (HashSet<RefPtr<Player> >::const_iterator iter = players.begin(); iter != players.end(); ++iter)
            (*iter)->cancel();
    }

    // FIXME: Apply updates to play-state.

    for (Vector<CSSAnimationUpdate::NewAnimation>::const_iterator iter = update->newAnimations().begin(); iter != update->newAnimations().end(); ++iter) {
        OwnPtr<AnimationEventDelegate> eventDelegate = adoptPtr(new AnimationEventDelegate(element, iter->name));
        HashSet<RefPtr<Player> > players;
        for (HashSet<RefPtr<InertAnimation> >::const_iterator animationsIter = iter->animations.begin(); animationsIter != iter->animations.end(); ++animationsIter) {
            const InertAnimation* inertAnimation = animationsIter->get();
            // The event delegate is set on the the first animation only. We
            // rely on the behavior of OwnPtr::release() to achieve this.
            RefPtr<Animation> animation = Animation::create(element, inertAnimation->effect(), inertAnimation->specified(), eventDelegate.release());
            players.add(element->document().timeline()->play(animation.get()));
        }
        m_animations.set(iter->name, players);
    }

    for (HashSet<CSSPropertyID>::iterator iter = update->cancelledTransitions().begin(); iter != update->cancelledTransitions().end(); ++iter) {
        ASSERT(m_transitions.contains(*iter));
        m_transitions.take(*iter).player->cancel();
    }

    for (size_t i = 0; i < update->newTransitions().size(); ++i) {
        const CSSAnimationUpdate::NewTransition& newTransition = update->newTransitions()[i];

        RunningTransition runningTransition;
        runningTransition.from = newTransition.from;
        runningTransition.to = newTransition.to;

        CSSPropertyID id = newTransition.id;
        InertAnimation* inertAnimation = newTransition.animation.get();
        OwnPtr<TransitionEventDelegate> eventDelegate = adoptPtr(new TransitionEventDelegate(element, id));
        RefPtr<Animation> transition = Animation::create(element, inertAnimation->effect(), inertAnimation->specified(), eventDelegate.release());
        // FIXME: Transitions need to be added to a separate timeline.
        runningTransition.player = element->document().timeline()->play(transition.get());
        m_transitions.set(id, runningTransition);
    }
}

void CSSAnimations::calculateTransitionUpdateForProperty(CSSAnimationUpdate* update, CSSPropertyID id, const CandidateTransition& newTransition, const TransitionMap* existingTransitions)
{
    // FIXME: Skip the rest of this if there is a running animation on this property

    if (existingTransitions) {
        TransitionMap::const_iterator existingTransitionIter = existingTransitions->find(id);

        if (existingTransitionIter != existingTransitions->end() && !update->cancelledTransitions().contains(id)) {
            const AnimatableValue* existingTo = existingTransitionIter->value.to;
            if (newTransition.to->equals(existingTo))
                return;
            update->cancelTransition(id);
        }
    }

    KeyframeAnimationEffect::KeyframeVector keyframes;

    RefPtr<Keyframe> startKeyframe = Keyframe::create();
    startKeyframe->setPropertyValue(id, newTransition.from.get());
    startKeyframe->setOffset(0);
    keyframes.append(startKeyframe);

    RefPtr<Keyframe> endKeyframe = Keyframe::create();
    endKeyframe->setPropertyValue(id, newTransition.to.get());
    endKeyframe->setOffset(1);
    keyframes.append(endKeyframe);

    RefPtr<KeyframeAnimationEffect> effect = KeyframeAnimationEffect::create(keyframes);

    Timing timing;
    RefPtr<TimingFunction> timingFunction = timingFromAnimationData(newTransition.anim, timing);
    timing.timingFunction = timingFunction;
    // Note that the backwards part is required for delay to work.
    timing.fillMode = Timing::FillModeBoth;

    update->startTransition(id, newTransition.from.get(), newTransition.to.get(), InertAnimation::create(effect, timing));
}

void CSSAnimations::calculateTransitionUpdate(CSSAnimationUpdate* update, const Element* element, const RenderStyle* style)
{
    ActiveAnimations* activeAnimations = element->activeAnimations();
    const CSSAnimations* cssAnimations = activeAnimations ? activeAnimations->cssAnimations() : 0;
    const TransitionMap* transitions = cssAnimations ? &cssAnimations->m_transitions : 0;

    HashSet<CSSPropertyID> listedProperties;
    if (style->display() != NONE && element->renderer() && element->renderer()->style()) {
        CandidateTransitionMap candidateMap;
        computeCandidateTransitions(element->renderer()->style(), style, candidateMap, listedProperties);
        for (CandidateTransitionMap::const_iterator iter = candidateMap.begin(); iter != candidateMap.end(); ++iter)
            calculateTransitionUpdateForProperty(update, iter->key, iter->value, transitions);
    }

    if (transitions) {
        for (TransitionMap::const_iterator iter = transitions->begin(); iter != transitions->end(); ++iter) {
            const TimedItem* timedItem = iter->value.player->source();
            CSSPropertyID id = iter->key;
            if (timedItem->phase() == TimedItem::PhaseAfter || !listedProperties.contains(id))
                update->cancelTransition(id);
        }
    }
}

void CSSAnimations::cancel()
{
    for (AnimationMap::iterator iter = m_animations.begin(); iter != m_animations.end(); ++iter) {
        const HashSet<RefPtr<Player> >& players = iter->value;
        for (HashSet<RefPtr<Player> >::const_iterator animationsIter = players.begin(); animationsIter != players.end(); ++animationsIter)
            (*animationsIter)->cancel();
    }

    for (TransitionMap::iterator iter = m_transitions.begin(); iter != m_transitions.end(); ++iter)
        iter->value.player->cancel();

    m_animations.clear();
    m_transitions.clear();
    m_pendingUpdate = nullptr;
}

void CSSAnimations::AnimationEventDelegate::maybeDispatch(Document::ListenerType listenerType, const AtomicString& eventName, double elapsedTime)
{
    if (m_target->document().hasListenerType(listenerType))
        m_target->document().timeline()->addEventToDispatch(m_target, WebKitAnimationEvent::create(eventName, m_name, elapsedTime));
}

void CSSAnimations::AnimationEventDelegate::onEventCondition(const TimedItem* timedItem, bool isFirstSample, TimedItem::Phase previousPhase, double previousIteration)
{
    // Events for a single document are queued and dispatched as a group at
    // the end of DocumentTimeline::serviceAnimations.
    // FIXME: Events which are queued outside of serviceAnimations should
    // trigger a timer to dispatch when control is released.
    const TimedItem::Phase currentPhase = timedItem->phase();
    const double currentIteration = timedItem->currentIteration();

    // Note that the elapsedTime is measured from when the animation starts playing.
    if (!isFirstSample && previousPhase == TimedItem::PhaseActive && currentPhase == TimedItem::PhaseActive && previousIteration != currentIteration) {
        ASSERT(!isNull(previousIteration));
        ASSERT(!isNull(currentIteration));
        // We fire only a single event for all iterations thast terminate
        // between a single pair of samples. See http://crbug.com/275263. For
        // compatibility with the existing implementation, this event uses
        // the elapsedTime for the first iteration in question.
        ASSERT(timedItem->specified().hasIterationDuration);
        const double elapsedTime = timedItem->specified().iterationDuration * (previousIteration + 1);
        maybeDispatch(Document::ANIMATIONITERATION_LISTENER, EventTypeNames::animationiteration, elapsedTime);
        return;
    }
    if ((isFirstSample || previousPhase == TimedItem::PhaseBefore) && isLaterPhase(currentPhase, TimedItem::PhaseBefore)) {
        ASSERT(timedItem->specified().startDelay > 0 || isFirstSample);
        // The spec states that the elapsed time should be
        // 'delay < 0 ? -delay : 0', but we always use 0 to match the existing
        // implementation. See crbug.com/279611
        maybeDispatch(Document::ANIMATIONSTART_LISTENER, EventTypeNames::animationstart, 0);
    }
    if ((isFirstSample || isEarlierPhase(previousPhase, TimedItem::PhaseAfter)) && currentPhase == TimedItem::PhaseAfter)
        maybeDispatch(Document::ANIMATIONEND_LISTENER, EventTypeNames::animationend, timedItem->activeDuration());
}

void CSSAnimations::TransitionEventDelegate::onEventCondition(const TimedItem* timedItem, bool isFirstSample, TimedItem::Phase previousPhase, double previousIteration)
{
    // Events for a single document are queued and dispatched as a group at
    // the end of DocumentTimeline::serviceAnimations.
    // FIXME: Events which are queued outside of serviceAnimations should
    // trigger a timer to dispatch when control is released.
    const TimedItem::Phase currentPhase = timedItem->phase();
    if (currentPhase == TimedItem::PhaseAfter && (isFirstSample || previousPhase != currentPhase) && m_target->document().hasListenerType(Document::TRANSITIONEND_LISTENER)) {
        String propertyName = getPropertyNameString(m_property);
        const Timing& timing = timedItem->specified();
        double elapsedTime = timing.iterationDuration;
        const AtomicString& eventType = EventTypeNames::transitionend;
        String pseudoElement = PseudoElement::pseudoElementNameForEvents(m_target->pseudoId());
        m_target->document().timeline()->addEventToDispatch(m_target, TransitionEvent::create(eventType, propertyName, elapsedTime, pseudoElement));
    }
}


bool CSSAnimations::isAnimatableProperty(CSSPropertyID property)
{
    switch (property) {
    case CSSPropertyBackgroundColor:
    case CSSPropertyBackgroundImage:
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyBackgroundSize:
    case CSSPropertyBaselineShift:
    case CSSPropertyBorderBottomColor:
    case CSSPropertyBorderBottomLeftRadius:
    case CSSPropertyBorderBottomRightRadius:
    case CSSPropertyBorderBottomWidth:
    case CSSPropertyBorderImageOutset:
    case CSSPropertyBorderImageSlice:
    case CSSPropertyBorderImageSource:
    case CSSPropertyBorderImageWidth:
    case CSSPropertyBorderLeftColor:
    case CSSPropertyBorderLeftWidth:
    case CSSPropertyBorderRightColor:
    case CSSPropertyBorderRightWidth:
    case CSSPropertyBorderTopColor:
    case CSSPropertyBorderTopLeftRadius:
    case CSSPropertyBorderTopRightRadius:
    case CSSPropertyBorderTopWidth:
    case CSSPropertyBottom:
    case CSSPropertyBoxShadow:
    case CSSPropertyClip:
    case CSSPropertyColor:
    case CSSPropertyFill:
    case CSSPropertyFillOpacity:
    // FIXME: Shorthands should not be present in this list, but
    // CSSPropertyAnimation implements animation of flex directly and
    // makes use of this method.
    case CSSPropertyFlex:
    case CSSPropertyFlexBasis:
    case CSSPropertyFlexGrow:
    case CSSPropertyFlexShrink:
    case CSSPropertyFloodColor:
    case CSSPropertyFloodOpacity:
    case CSSPropertyFontSize:
    case CSSPropertyHeight:
    case CSSPropertyKerning:
    case CSSPropertyLeft:
    case CSSPropertyLetterSpacing:
    case CSSPropertyLightingColor:
    case CSSPropertyLineHeight:
    case CSSPropertyListStyleImage:
    case CSSPropertyMarginBottom:
    case CSSPropertyMarginLeft:
    case CSSPropertyMarginRight:
    case CSSPropertyMarginTop:
    case CSSPropertyMaxHeight:
    case CSSPropertyMaxWidth:
    case CSSPropertyMinHeight:
    case CSSPropertyMinWidth:
    case CSSPropertyObjectPosition:
    case CSSPropertyOpacity:
    case CSSPropertyOrphans:
    case CSSPropertyOutlineColor:
    case CSSPropertyOutlineOffset:
    case CSSPropertyOutlineWidth:
    case CSSPropertyPaddingBottom:
    case CSSPropertyPaddingLeft:
    case CSSPropertyPaddingRight:
    case CSSPropertyPaddingTop:
    case CSSPropertyRight:
    case CSSPropertyStopColor:
    case CSSPropertyStopOpacity:
    case CSSPropertyStroke:
    case CSSPropertyStrokeDasharray:
    case CSSPropertyStrokeDashoffset:
    case CSSPropertyStrokeMiterlimit:
    case CSSPropertyStrokeOpacity:
    case CSSPropertyStrokeWidth:
    case CSSPropertyTextIndent:
    case CSSPropertyTextShadow:
    case CSSPropertyTop:
    case CSSPropertyVisibility:
    case CSSPropertyWebkitBackgroundSize:
    case CSSPropertyWebkitBorderHorizontalSpacing:
    case CSSPropertyWebkitBorderVerticalSpacing:
    case CSSPropertyWebkitBoxShadow:
    case CSSPropertyWebkitClipPath:
    case CSSPropertyWebkitColumnCount:
    case CSSPropertyWebkitColumnGap:
    case CSSPropertyWebkitColumnRuleColor:
    case CSSPropertyWebkitColumnRuleWidth:
    case CSSPropertyWebkitColumnWidth:
    case CSSPropertyWebkitFilter:
    case CSSPropertyWebkitMaskBoxImage:
    case CSSPropertyWebkitMaskBoxImageSource:
    case CSSPropertyWebkitMaskImage:
    case CSSPropertyWebkitMaskPositionX:
    case CSSPropertyWebkitMaskPositionY:
    case CSSPropertyWebkitMaskSize:
    case CSSPropertyWebkitPerspective:
    case CSSPropertyWebkitPerspectiveOriginX:
    case CSSPropertyWebkitPerspectiveOriginY:
    case CSSPropertyWebkitShapeInside:
    case CSSPropertyWebkitShapeOutside:
    case CSSPropertyWebkitTextStrokeColor:
    case CSSPropertyWebkitTransform:
    case CSSPropertyWebkitTransformOriginX:
    case CSSPropertyWebkitTransformOriginY:
    case CSSPropertyWebkitTransformOriginZ:
    case CSSPropertyWidows:
    case CSSPropertyWidth:
    case CSSPropertyWordSpacing:
    case CSSPropertyZIndex:
    case CSSPropertyZoom:
        return true;
    default:
        return false;
    }
}

const StylePropertyShorthand& CSSAnimations::animatableProperties()
{
    DEFINE_STATIC_LOCAL(Vector<CSSPropertyID>, properties, ());
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, propertyShorthand, ());
    if (properties.isEmpty()) {
        for (int i = firstCSSProperty; i < lastCSSProperty; ++i) {
            CSSPropertyID id = convertToCSSPropertyID(i);
            // FIXME: This is the only shorthand marked as animatable,
            // it'll be removed from the list once we switch to the new implementation.
            if (id == CSSPropertyFlex)
                continue;
            if (isAnimatableProperty(id))
                properties.append(id);
        }
        propertyShorthand = StylePropertyShorthand(CSSPropertyInvalid, properties.begin(), properties.size());
    }
    return propertyShorthand;
}

} // namespace WebCore
