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
#include "core/animation/CompositorAnimations.h"
#include "core/animation/DocumentTimeline.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/animation/css/CSSAnimatableValueFactory.h"
#include "core/animation/css/CSSAnimationDataList.h"
#include "core/animation/css/CSSPropertyEquality.h"
#include "core/css/CSSKeyframeRule.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Element.h"
#include "core/dom/PseudoElement.h"
#include "core/events/ThreadLocalEventNames.h"
#include "core/events/TransitionEvent.h"
#include "core/events/WebKitAnimationEvent.h"
#include "core/frame/UseCounter.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderObject.h"
#include "core/rendering/style/KeyframeList.h"
#include "platform/animation/TimingFunction.h"
#include "public/platform/Platform.h"
#include "wtf/BitArray.h"
#include "wtf/HashSet.h"

namespace WebCore {

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

static PassRefPtr<TimingFunction> generateTimingFunction(const KeyframeEffectModel::KeyframeVector keyframes, const HashMap<double, RefPtr<TimingFunction> > perKeyframeTimingFunctions)
{
    // Generate the chained timing function. Note that timing functions apply
    // from the keyframe in which they're specified to the next keyframe.
    bool isTimingFunctionLinearThroughout = true;
    RefPtr<ChainedTimingFunction> chainedTimingFunction = ChainedTimingFunction::create();
    for (size_t i = 0; i < keyframes.size() - 1; ++i) {
        double lowerBound = keyframes[i]->offset();
        ASSERT(lowerBound >=0 && lowerBound < 1);
        double upperBound = keyframes[i + 1]->offset();
        ASSERT(upperBound > 0 && upperBound <= 1);
        TimingFunction* timingFunction = perKeyframeTimingFunctions.get(lowerBound);
        isTimingFunctionLinearThroughout &= timingFunction->type() == TimingFunction::LinearFunction;
        chainedTimingFunction->appendSegment(upperBound, timingFunction);
    }
    if (isTimingFunctionLinearThroughout)
        return LinearTimingFunction::create();
    return chainedTimingFunction;
}

static void resolveKeyframes(StyleResolver* resolver, Element* element, const Element& parentElement, const RenderStyle& style, RenderStyle* parentStyle, const AtomicString& name, TimingFunction* defaultTimingFunction,
    WillBeHeapVector<std::pair<KeyframeEffectModel::KeyframeVector, RefPtr<TimingFunction> > >& keyframesAndTimingFunctions)
{
    // When the element is null, use its parent for scoping purposes.
    const Element* elementForScoping = element ? element : &parentElement;
    const StyleRuleKeyframes* keyframesRule = CSSAnimations::matchScopedKeyframesRule(resolver, elementForScoping, name.impl());
    if (!keyframesRule)
        return;

    const Vector<RefPtr<StyleKeyframe> >& styleKeyframes = keyframesRule->keyframes();
    if (styleKeyframes.isEmpty())
        return;

    // Construct and populate the style for each keyframe
    PropertySet specifiedProperties;
    KeyframeEffectModel::KeyframeVector keyframes;
    HashMap<double, RefPtr<TimingFunction> > perKeyframeTimingFunctions;
    for (size_t i = 0; i < styleKeyframes.size(); ++i) {
        const StyleKeyframe* styleKeyframe = styleKeyframes[i].get();
        // It's OK to pass a null element here.
        RefPtr<RenderStyle> keyframeStyle = resolver->styleForKeyframe(element, style, parentStyle, styleKeyframe, name);
        RefPtrWillBeRawPtr<Keyframe> keyframe = Keyframe::create();
        const Vector<double>& offsets = styleKeyframe->keys();
        ASSERT(!offsets.isEmpty());
        keyframe->setOffset(offsets[0]);
        TimingFunction* timingFunction = defaultTimingFunction;
        const StylePropertySet& properties = styleKeyframe->properties();
        for (unsigned j = 0; j < properties.propertyCount(); j++) {
            CSSPropertyID property = properties.propertyAt(j).id();
            specifiedProperties.add(property);
            if (property == CSSPropertyWebkitAnimationTimingFunction || property == CSSPropertyAnimationTimingFunction)
                timingFunction = KeyframeValue::timingFunction(*keyframeStyle);
            else if (CSSAnimations::isAnimatableProperty(property))
                keyframe->setPropertyValue(property, CSSAnimatableValueFactory::create(property, *keyframeStyle).get());
        }
        keyframes.append(keyframe);
        // The last keyframe specified at a given offset is used.
        perKeyframeTimingFunctions.set(offsets[0], timingFunction);
        for (size_t j = 1; j < offsets.size(); ++j) {
            keyframes.append(keyframe->cloneWithOffset(offsets[j]));
            perKeyframeTimingFunctions.set(offsets[j], timingFunction);
        }
    }
    ASSERT(!keyframes.isEmpty());

    if (!perKeyframeTimingFunctions.contains(0))
        perKeyframeTimingFunctions.set(0, defaultTimingFunction);

    for (PropertySet::const_iterator iter = specifiedProperties.begin(); iter != specifiedProperties.end(); ++iter) {
        const CSSPropertyID property = *iter;
        ASSERT(property != CSSPropertyInvalid);
        blink::Platform::current()->histogramSparse("WebCore.Animation.CSSProperties", UseCounter::mapCSSPropertyIdToCSSSampleIdForHistogram(property));
    }

    // Remove duplicate keyframes. In CSS the last keyframe at a given offset takes priority.
    std::stable_sort(keyframes.begin(), keyframes.end(), Keyframe::compareOffsets);
    size_t targetIndex = 0;
    for (size_t i = 1; i < keyframes.size(); i++) {
        if (keyframes[i]->offset() != keyframes[targetIndex]->offset())
            targetIndex++;
        if (targetIndex != i)
            keyframes[targetIndex] = keyframes[i];
    }
    keyframes.shrink(targetIndex + 1);

    // Add 0% and 100% keyframes if absent.
    RefPtrWillBeRawPtr<Keyframe> startKeyframe = keyframes[0];
    if (startKeyframe->offset()) {
        startKeyframe = Keyframe::create();
        startKeyframe->setOffset(0);
        keyframes.prepend(startKeyframe);
    }
    RefPtrWillBeRawPtr<Keyframe> endKeyframe = keyframes[keyframes.size() - 1];
    if (endKeyframe->offset() != 1) {
        endKeyframe = Keyframe::create();
        endKeyframe->setOffset(1);
        keyframes.append(endKeyframe);
    }
    ASSERT(keyframes.size() >= 2);
    ASSERT(!keyframes.first()->offset());
    ASSERT(keyframes.last()->offset() == 1);

    // Snapshot current property values for 0% and 100% if missing.
    PropertySet allProperties;
    size_t numKeyframes = keyframes.size();
    for (size_t i = 0; i < numKeyframes; i++) {
        const PropertySet& keyframeProperties = keyframes[i]->properties();
        for (PropertySet::const_iterator iter = keyframeProperties.begin(); iter != keyframeProperties.end(); ++iter)
            allProperties.add(*iter);
    }
    const PropertySet& startKeyframeProperties = startKeyframe->properties();
    const PropertySet& endKeyframeProperties = endKeyframe->properties();
    bool missingStartValues = startKeyframeProperties.size() < allProperties.size();
    bool missingEndValues = endKeyframeProperties.size() < allProperties.size();
    if (missingStartValues || missingEndValues) {
        for (PropertySet::const_iterator iter = allProperties.begin(); iter != allProperties.end(); ++iter) {
            const CSSPropertyID property = *iter;
            bool startNeedsValue = missingStartValues && !startKeyframeProperties.contains(property);
            bool endNeedsValue = missingEndValues && !endKeyframeProperties.contains(property);
            if (!startNeedsValue && !endNeedsValue)
                continue;
            RefPtr<AnimatableValue> snapshotValue = CSSAnimatableValueFactory::create(property, style);
            if (startNeedsValue)
                startKeyframe->setPropertyValue(property, snapshotValue.get());
            if (endNeedsValue)
                endKeyframe->setPropertyValue(property, snapshotValue.get());
        }
    }
    ASSERT(startKeyframe->properties().size() == allProperties.size());
    ASSERT(endKeyframe->properties().size() == allProperties.size());

    // Determine how many keyframes specify each property. Note that this must
    // be done after we've filled in end keyframes.
    typedef HashCountedSet<CSSPropertyID> PropertyCountedSet;
    PropertyCountedSet propertyCounts;
    for (size_t i = 0; i < numKeyframes; ++i) {
        const PropertySet& properties = keyframes[i]->properties();
        for (PropertySet::const_iterator iter = properties.begin(); iter != properties.end(); ++iter)
            propertyCounts.add(*iter);
    }

    // Split keyframes into groups, where each group contains only keyframes
    // which specify all properties used in that group. Each group is animated
    // in a separate animation, to allow per-keyframe timing functions to be
    // applied correctly.
    for (PropertyCountedSet::const_iterator iter = propertyCounts.begin(); iter != propertyCounts.end(); ++iter) {
        const CSSPropertyID property = iter->key;
        const size_t count = iter->value;
        ASSERT(count <= numKeyframes);
        if (count == numKeyframes)
            continue;
        KeyframeEffectModel::KeyframeVector splitOutKeyframes;
        for (size_t i = 0; i < numKeyframes; i++) {
            Keyframe* keyframe = keyframes[i].get();
            if (!keyframe->properties().contains(property)) {
                ASSERT(i && i != numKeyframes - 1);
                continue;
            }
            RefPtrWillBeRawPtr<Keyframe> clonedKeyframe = Keyframe::create();
            clonedKeyframe->setOffset(keyframe->offset());
            clonedKeyframe->setComposite(keyframe->composite());
            clonedKeyframe->setPropertyValue(property, keyframe->propertyValue(property));
            splitOutKeyframes.append(clonedKeyframe);
            // Note that it's OK if this keyframe ends up having no
            // properties. This can only happen when none of the properties
            // are specified in all keyframes, in which case we won't animate
            // anything with these keyframes.
            keyframe->clearPropertyValue(property);
        }
        ASSERT(!splitOutKeyframes.first()->offset());
        ASSERT(splitOutKeyframes.last()->offset() == 1);
#ifndef NDEBUG
        for (size_t j = 0; j < splitOutKeyframes.size(); ++j)
            ASSERT(splitOutKeyframes[j]->properties().size() == 1);
#endif
        keyframesAndTimingFunctions.append(std::make_pair(splitOutKeyframes, generateTimingFunction(splitOutKeyframes, perKeyframeTimingFunctions)));
    }

    unsigned numPropertiesSpecifiedInAllKeyframes = keyframes.first()->properties().size();
#ifndef NDEBUG
    for (size_t i = 1; i < numKeyframes; ++i)
        ASSERT(keyframes[i]->properties().size() == numPropertiesSpecifiedInAllKeyframes);
#endif

    // If the animation specifies any keyframes, we always provide at least one
    // vector of resolved keyframes, even if no properties are animated.
    if (numPropertiesSpecifiedInAllKeyframes || keyframesAndTimingFunctions.isEmpty())
        keyframesAndTimingFunctions.append(std::make_pair(keyframes, generateTimingFunction(keyframes, perKeyframeTimingFunctions)));
}

// Returns the default timing function.
const PassRefPtr<TimingFunction> timingFromAnimationData(const CSSAnimationData* animationData, Timing& timing, bool& isPaused)
{
    if (animationData->isDelaySet())
        timing.startDelay = animationData->delay();
    if (animationData->isDurationSet())
        timing.iterationDuration = animationData->duration();
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

    // For CSS, the constraints on the timing properties are tighter than in
    // the general case of the Web Animations model.
    timing.assertValid();
    ASSERT(!timing.iterationStart);
    ASSERT(timing.playbackRate == 1);
    ASSERT(!std::isinf(timing.iterationDuration));

    isPaused = animationData->isPlayStateSet() && animationData->playState() == AnimPlayStatePaused;
    return animationData->isTimingFunctionSet() ? animationData->timingFunction() : CSSAnimationData::initialAnimationTimingFunction();
}

} // namespace

const StyleRuleKeyframes* CSSAnimations::matchScopedKeyframesRule(StyleResolver* resolver, const Element* element, const StringImpl* animationName)
{
    if (resolver->styleTreeHasOnlyScopedResolverForDocument())
        return resolver->styleTreeScopedStyleResolverForDocument()->keyframeStylesForAnimation(animationName);

    Vector<ScopedStyleResolver*, 8> stack;
    resolver->styleTreeResolveScopedKeyframesRules(element, stack);
    if (stack.isEmpty())
        return 0;

    for (size_t i = 0; i < stack.size(); ++i) {
        if (const StyleRuleKeyframes* keyframesRule = stack.at(i)->keyframeStylesForAnimation(animationName))
            return keyframesRule;
    }
    return 0;
}

PassOwnPtr<CSSAnimationUpdate> CSSAnimations::calculateUpdate(Element* element, const Element& parentElement, const RenderStyle& style, RenderStyle* parentStyle, StyleResolver* resolver)
{
    OwnPtr<CSSAnimationUpdate> update = adoptPtr(new CSSAnimationUpdate());
    calculateAnimationUpdate(update.get(), element, parentElement, style, parentStyle, resolver);
    calculateAnimationCompositableValues(update.get(), element);
    calculateTransitionUpdate(update.get(), element, style);
    calculateTransitionCompositableValues(update.get(), element);
    return update->isEmpty() ? nullptr : update.release();
}

void CSSAnimations::calculateAnimationUpdate(CSSAnimationUpdate* update, Element* element, const Element& parentElement, const RenderStyle& style, RenderStyle* parentStyle, StyleResolver* resolver)
{
    const ActiveAnimations* activeAnimations = element ? element->activeAnimations() : 0;

#if ASSERT_DISABLED
    // If we're in an animation style change, no animations can have started, been cancelled or changed play state.
    // When ASSERT is enabled, we verify this optimization.
    if (activeAnimations && activeAnimations->isAnimationStyleChange())
        return;
#endif

    const CSSAnimationDataList* animationDataList = style.animations();
    const CSSAnimations* cssAnimations = activeAnimations ? &activeAnimations->cssAnimations() : 0;

    HashSet<AtomicString> inactive;
    if (cssAnimations)
        for (AnimationMap::const_iterator iter = cssAnimations->m_animations.begin(); iter != cssAnimations->m_animations.end(); ++iter)
            inactive.add(iter->key);

    if (style.display() != NONE) {
        for (size_t i = 0; animationDataList && i < animationDataList->size(); ++i) {
            const CSSAnimationData* animationData = animationDataList->animation(i);
            if (animationData->isNoneAnimation())
                continue;
            ASSERT(animationData->isValidAnimation());
            AtomicString animationName(animationData->name());

            // Keyframes and animation properties are snapshotted when the
            // animation starts, so we don't need to track changes to these,
            // with the exception of play-state.
            if (cssAnimations) {
                AnimationMap::const_iterator existing(cssAnimations->m_animations.find(animationName));
                if (existing != cssAnimations->m_animations.end()) {
                    inactive.remove(animationName);
                    const HashSet<RefPtr<Player> >& players = existing->value;
                    ASSERT(!players.isEmpty());
                    bool isFirstPlayerPaused = (*players.begin())->paused();
#ifndef NDEBUG
                    for (HashSet<RefPtr<Player> >::const_iterator iter = players.begin(); iter != players.end(); ++iter)
                        ASSERT((*iter)->paused() == isFirstPlayerPaused);
#endif
                    if ((animationData->playState() == AnimPlayStatePaused) != isFirstPlayerPaused) {
                        ASSERT(!activeAnimations || !activeAnimations->isAnimationStyleChange());
                        update->toggleAnimationPaused(animationName);
                    }
                    continue;
                }
            }

            Timing timing;
            bool isPaused;
            RefPtr<TimingFunction> defaultTimingFunction = timingFromAnimationData(animationData, timing, isPaused);
            WillBeHeapVector<std::pair<KeyframeEffectModel::KeyframeVector, RefPtr<TimingFunction> > > keyframesAndTimingFunctions;
            resolveKeyframes(resolver, element, parentElement, style, parentStyle, animationName, defaultTimingFunction.get(), keyframesAndTimingFunctions);
            if (!keyframesAndTimingFunctions.isEmpty()) {
                HashSet<RefPtr<InertAnimation> > animations;
                for (size_t j = 0; j < keyframesAndTimingFunctions.size(); ++j) {
                    ASSERT(!keyframesAndTimingFunctions[j].first.isEmpty());
                    timing.timingFunction = keyframesAndTimingFunctions[j].second;
                    // FIXME: crbug.com/268791 - Keyframes are already normalized, perhaps there should be a flag on KeyframeEffectModel to skip normalization.
                    animations.add(InertAnimation::create(KeyframeEffectModel::create(keyframesAndTimingFunctions[j].first), timing, isPaused));
                }
                ASSERT(!activeAnimations || !activeAnimations->isAnimationStyleChange());
                update->startAnimation(animationName, animations);
            }
        }
    }

    ASSERT(inactive.isEmpty() || cssAnimations);
    for (HashSet<AtomicString>::const_iterator iter = inactive.begin(); iter != inactive.end(); ++iter) {
        ASSERT(!activeAnimations || !activeAnimations->isAnimationStyleChange());
        update->cancelAnimation(*iter, cssAnimations->m_animations.get(*iter));
    }
}

void CSSAnimations::maybeApplyPendingUpdate(Element* element)
{
    if (!m_pendingUpdate) {
        m_previousCompositableValuesForAnimations.clear();
        return;
    }

    OwnPtr<CSSAnimationUpdate> update = m_pendingUpdate.release();

    m_previousCompositableValuesForAnimations.swap(update->compositableValuesForAnimations());

    // FIXME: cancelling, pausing, unpausing animations all query compositingState, which is not necessarily up to date here
    // since we call this from recalc style.
    // https://code.google.com/p/chromium/issues/detail?id=339847
    DisableCompositingQueryAsserts disabler;

    for (Vector<AtomicString>::const_iterator iter = update->cancelledAnimationNames().begin(); iter != update->cancelledAnimationNames().end(); ++iter) {
        const HashSet<RefPtr<Player> >& players = m_animations.take(*iter);
        for (HashSet<RefPtr<Player> >::const_iterator iter = players.begin(); iter != players.end(); ++iter)
            (*iter)->cancel();
    }

    for (Vector<AtomicString>::const_iterator iter = update->animationsWithPauseToggled().begin(); iter != update->animationsWithPauseToggled().end(); ++iter) {
        const HashSet<RefPtr<Player> >& players = m_animations.get(*iter);
        ASSERT(!players.isEmpty());
        bool isFirstPlayerPaused = (*players.begin())->paused();
        for (HashSet<RefPtr<Player> >::const_iterator iter = players.begin(); iter != players.end(); ++iter) {
            Player* player = iter->get();
            ASSERT(player->paused() == isFirstPlayerPaused);
            if (isFirstPlayerPaused)
                player->unpause();
            else
                player->pause();
        }
    }

    for (Vector<CSSAnimationUpdate::NewAnimation>::const_iterator iter = update->newAnimations().begin(); iter != update->newAnimations().end(); ++iter) {
        OwnPtr<AnimationEventDelegate> eventDelegate = adoptPtr(new AnimationEventDelegate(element, iter->name));
        HashSet<RefPtr<Player> > players;
        for (HashSet<RefPtr<InertAnimation> >::const_iterator animationsIter = iter->animations.begin(); animationsIter != iter->animations.end(); ++animationsIter) {
            const InertAnimation* inertAnimation = animationsIter->get();
            // The event delegate is set on the the first animation only. We
            // rely on the behavior of OwnPtr::release() to achieve this.
            RefPtr<Animation> animation = Animation::create(element, inertAnimation->effect(), inertAnimation->specifiedTiming(), Animation::DefaultPriority, eventDelegate.release());
            Player* player = element->document().timeline().createPlayer(animation.get());
            if (inertAnimation->paused())
                player->pause();
            element->document().cssPendingAnimations().add(player);
            player->update();
            players.add(player);
        }
        m_animations.set(iter->name, players);
    }

    // Transitions that are run on the compositor only update main-thread state
    // lazily. However, we need the new state to know what the from state shoud
    // be when transitions are retargeted. Instead of triggering complete style
    // recalculation, we find these cases by searching for new transitions that
    // have matching cancelled animation property IDs on the compositor.
    HashMap<CSSPropertyID, std::pair<RefPtr<Animation>, double> > retargetedCompositorTransitions;
    const ActiveAnimations* activeAnimations = element->activeAnimations();
    for (HashSet<CSSPropertyID>::iterator iter = update->cancelledTransitions().begin(); iter != update->cancelledTransitions().end(); ++iter) {
        CSSPropertyID id = *iter;
        ASSERT(m_transitions.contains(id));
        Player* player = m_transitions.take(id).transition->player();
        if (activeAnimations && activeAnimations->hasActiveAnimationsOnCompositor(id) && update->newTransitions().find(id) != update->newTransitions().end())
            retargetedCompositorTransitions.add(id, std::pair<RefPtr<Animation>, double>(toAnimation(player->source()), player->startTime()));
        player->cancel();
    }

    for (CSSAnimationUpdate::NewTransitionMap::const_iterator iter = update->newTransitions().begin(); iter != update->newTransitions().end(); ++iter) {
        const CSSAnimationUpdate::NewTransition& newTransition = iter->value;

        RunningTransition runningTransition;
        runningTransition.from = newTransition.from;
        runningTransition.to = newTransition.to;

        CSSPropertyID id = newTransition.id;
        InertAnimation* inertAnimation = newTransition.animation.get();
        OwnPtr<TransitionEventDelegate> eventDelegate = adoptPtr(new TransitionEventDelegate(element, id));

        RefPtrWillBeRawPtr<AnimationEffect> effect = inertAnimation->effect();

        if (retargetedCompositorTransitions.contains(id)) {
            const std::pair<RefPtr<Animation>, double>& oldTransition = retargetedCompositorTransitions.get(id);
            RefPtr<Animation> oldAnimation = oldTransition.first;
            double oldStartTime = oldTransition.second;
            double inheritedTime = isNull(oldStartTime) ? 0 : element->document().transitionTimeline().currentTime() - oldStartTime;
            oldAnimation->updateInheritedTime(inheritedTime);
            KeyframeEffectModel* oldEffect = toKeyframeEffectModel(inertAnimation->effect());
            const KeyframeEffectModel::KeyframeVector& frames = oldEffect->getFrames();
            KeyframeEffectModel::KeyframeVector newFrames;
            newFrames.append(frames[0]->clone());
            newFrames[0]->clearPropertyValue(id);
            ASSERT(oldAnimation->compositableValues()->size() == 1);
            const AnimationEffect::CompositableValue* compositableValue = oldAnimation->compositableValues()->at(0).second.get();
            ASSERT(!compositableValue->dependsOnUnderlyingValue());
            newFrames[0]->setPropertyValue(id, compositableValue->compositeOnto(0).get());
            newFrames.append(frames[1]->clone());
            effect = KeyframeEffectModel::create(newFrames);
        }
        RefPtr<Animation> transition = Animation::create(element, effect, inertAnimation->specifiedTiming(), Animation::TransitionPriority, eventDelegate.release());
        RefPtr<Player> player = element->document().transitionTimeline().createPlayer(transition.get());
        player->update();
        element->document().cssPendingAnimations().add(player.get());
        runningTransition.transition = transition.get();
        m_transitions.set(id, runningTransition);
        ASSERT(id != CSSPropertyInvalid);
        blink::Platform::current()->histogramSparse("WebCore.Animation.CSSProperties", UseCounter::mapCSSPropertyIdToCSSSampleIdForHistogram(id));
    }
}

void CSSAnimations::calculateTransitionUpdateForProperty(CSSPropertyID id, const CSSAnimationData* anim, const RenderStyle& oldStyle, const RenderStyle& style, const TransitionMap* activeTransitions, CSSAnimationUpdate* update, const Element* element)
{
    RefPtr<AnimatableValue> to;
    if (activeTransitions) {
        TransitionMap::const_iterator activeTransitionIter = activeTransitions->find(id);
        if (activeTransitionIter != activeTransitions->end()) {
            to = CSSAnimatableValueFactory::create(id, style);
            const AnimatableValue* activeTo = activeTransitionIter->value.to;
            if (to->equals(activeTo))
                return;
            update->cancelTransition(id);
            ASSERT(!element->activeAnimations() || !element->activeAnimations()->isAnimationStyleChange());
        }
    }

    if (anim->duration() + anim->delay() <= 0)
        return;

    if (CSSPropertyEquality::propertiesEqual(id, oldStyle, style))
        return;
    if (!to)
        to = CSSAnimatableValueFactory::create(id, style);

    RefPtr<AnimatableValue> from = CSSAnimatableValueFactory::create(id, oldStyle);
    // If we have multiple transitions on the same property, we will use the
    // last one since we iterate over them in order.
    if (AnimatableValue::usesDefaultInterpolation(to.get(), from.get()))
        return;

    KeyframeEffectModel::KeyframeVector keyframes;

    RefPtrWillBeRawPtr<Keyframe> startKeyframe = Keyframe::create();
    startKeyframe->setPropertyValue(id, from.get());
    startKeyframe->setOffset(0);
    keyframes.append(startKeyframe);

    RefPtrWillBeRawPtr<Keyframe> endKeyframe = Keyframe::create();
    endKeyframe->setPropertyValue(id, to.get());
    endKeyframe->setOffset(1);
    keyframes.append(endKeyframe);

    RefPtrWillBeRawPtr<KeyframeEffectModel> effect = KeyframeEffectModel::create(keyframes);

    Timing timing;
    bool isPaused;
    RefPtr<TimingFunction> timingFunction = timingFromAnimationData(anim, timing, isPaused);
    ASSERT(!isPaused);
    timing.timingFunction = timingFunction;
    // Note that the backwards part is required for delay to work.
    timing.fillMode = Timing::FillModeBoth;

    update->startTransition(id, from.get(), to.get(), InertAnimation::create(effect, timing, isPaused));
    ASSERT(!element->activeAnimations() || !element->activeAnimations()->isAnimationStyleChange());
}

void CSSAnimations::calculateTransitionUpdate(CSSAnimationUpdate* update, const Element* element, const RenderStyle& style)
{
    if (!element)
        return;

    ActiveAnimations* activeAnimations = element->activeAnimations();
    const TransitionMap* activeTransitions = activeAnimations ? &activeAnimations->cssAnimations().m_transitions : 0;

#if ASSERT_DISABLED
    // In release builds we avoid the cost of checking for new and interrupted transitions if the style recalc is due to animation.
    const bool animationStyleRecalc = activeAnimations && activeAnimations->isAnimationStyleChange();
#else
    // In debug builds we verify that it would have been safe to avoid populating and testing listedProperties if the style recalc is due to animation.
    const bool animationStyleRecalc = false;
#endif

    BitArray<numCSSProperties> listedProperties;
    bool anyTransitionHadAnimateAll = false;
    const RenderObject* renderer = element->renderer();
    if (!animationStyleRecalc && style.display() != NONE && renderer && renderer->style() && style.transitions()) {
        const RenderStyle& oldStyle = *renderer->style();

        for (size_t i = 0; i < style.transitions()->size(); ++i) {
            const CSSAnimationData* anim = style.transitions()->animation(i);
            CSSAnimationData::AnimationMode mode = anim->animationMode();
            if (mode == CSSAnimationData::AnimateNone)
                continue;

            bool animateAll = mode == CSSAnimationData::AnimateAll;
            ASSERT(animateAll || mode == CSSAnimationData::AnimateSingleProperty);
            if (animateAll)
                anyTransitionHadAnimateAll = true;
            const StylePropertyShorthand& propertyList = animateAll ? CSSAnimations::animatableProperties() : shorthandForProperty(anim->property());
            // If not a shorthand we only execute one iteration of this loop, and refer to the property directly.
            for (unsigned j = 0; !j || j < propertyList.length(); ++j) {
                CSSPropertyID id = propertyList.length() ? propertyList.properties()[j] : anim->property();

                if (!animateAll) {
                    if (CSSAnimations::isAnimatableProperty(id))
                        listedProperties.set(id);
                    else
                        continue;
                }

                // FIXME: We should transition if an !important property changes even when an animation is running,
                // but this is a bit hard to do with the current applyMatchedProperties system.
                if (!update->compositableValuesForAnimations().contains(id)
                    && (!activeAnimations || !activeAnimations->cssAnimations().m_previousCompositableValuesForAnimations.contains(id))) {
                    calculateTransitionUpdateForProperty(id, anim, oldStyle, style, activeTransitions, update, element);
                }
            }
        }
    }

    if (activeTransitions) {
        for (TransitionMap::const_iterator iter = activeTransitions->begin(); iter != activeTransitions->end(); ++iter) {
            const TimedItem* timedItem = iter->value.transition;
            CSSPropertyID id = iter->key;
            if (timedItem->phase() == TimedItem::PhaseAfter || (!anyTransitionHadAnimateAll && !animationStyleRecalc && !listedProperties.get(id))) {
                ASSERT(timedItem->phase() == TimedItem::PhaseAfter || !(activeAnimations && activeAnimations->isAnimationStyleChange()));
                update->cancelTransition(id);
            }
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
        iter->value.transition->player()->cancel();

    m_animations.clear();
    m_transitions.clear();
    m_pendingUpdate = nullptr;
}

void CSSAnimations::calculateAnimationCompositableValues(CSSAnimationUpdate* update, const Element* element)
{
    ActiveAnimations* activeAnimations = element ? element->activeAnimations() : 0;
    AnimationStack* animationStack = activeAnimations ? &activeAnimations->defaultStack() : 0;

    if (update->newAnimations().isEmpty() && update->cancelledAnimationPlayers().isEmpty()) {
        AnimationEffect::CompositableValueMap compositableValuesForAnimations(AnimationStack::compositableValues(animationStack, 0, 0, Animation::DefaultPriority));
        update->adoptCompositableValuesForAnimations(compositableValuesForAnimations);
        return;
    }

    Vector<InertAnimation*> newAnimations;
    for (size_t i = 0; i < update->newAnimations().size(); ++i) {
        HashSet<RefPtr<InertAnimation> > animations = update->newAnimations()[i].animations;
        for (HashSet<RefPtr<InertAnimation> >::const_iterator animationsIter = animations.begin(); animationsIter != animations.end(); ++animationsIter)
            newAnimations.append(animationsIter->get());
    }
    AnimationEffect::CompositableValueMap compositableValuesForAnimations(AnimationStack::compositableValues(animationStack, &newAnimations, &update->cancelledAnimationPlayers(), Animation::DefaultPriority));
    update->adoptCompositableValuesForAnimations(compositableValuesForAnimations);
}

void CSSAnimations::calculateTransitionCompositableValues(CSSAnimationUpdate* update, const Element* element)
{
    ActiveAnimations* activeAnimations = element ? element->activeAnimations() : 0;
    AnimationStack* animationStack = activeAnimations ? &activeAnimations->defaultStack() : 0;

    AnimationEffect::CompositableValueMap compositableValuesForTransitions;
    if (update->newTransitions().isEmpty() && update->cancelledTransitions().isEmpty()) {
        compositableValuesForTransitions = AnimationStack::compositableValues(animationStack, 0, 0, Animation::TransitionPriority);
    } else {
        Vector<InertAnimation*> newTransitions;
        for (CSSAnimationUpdate::NewTransitionMap::const_iterator iter = update->newTransitions().begin(); iter != update->newTransitions().end(); ++iter)
            newTransitions.append(iter->value.animation.get());

        HashSet<const Player*> cancelledPlayers;
        if (!update->cancelledTransitions().isEmpty()) {
            ASSERT(activeAnimations);
            const TransitionMap& transitionMap = activeAnimations->cssAnimations().m_transitions;
            for (HashSet<CSSPropertyID>::iterator iter = update->cancelledTransitions().begin(); iter != update->cancelledTransitions().end(); ++iter) {
                ASSERT(transitionMap.contains(*iter));
                cancelledPlayers.add(transitionMap.get(*iter).transition->player());
            }
        }

        compositableValuesForTransitions = AnimationStack::compositableValues(animationStack, &newTransitions, &cancelledPlayers, Animation::TransitionPriority);
    }

    // Properties being animated by animations don't get values from transitions applied.
    if (!update->compositableValuesForAnimations().isEmpty() && !compositableValuesForTransitions.isEmpty()) {
        for (AnimationEffect::CompositableValueMap::const_iterator iter = update->compositableValuesForAnimations().begin(); iter != update->compositableValuesForAnimations().end(); ++iter)
            compositableValuesForTransitions.remove(iter->key);
    }
    update->adoptCompositableValuesForTransitions(compositableValuesForTransitions);
}

void CSSAnimations::AnimationEventDelegate::maybeDispatch(Document::ListenerType listenerType, const AtomicString& eventName, double elapsedTime)
{
    if (m_target->document().hasListenerType(listenerType)) {
        RefPtr<WebKitAnimationEvent> event = WebKitAnimationEvent::create(eventName, m_name, elapsedTime);
        event->setTarget(m_target);
        m_target->document().enqueueAnimationFrameEvent(event);
    }
}

void CSSAnimations::AnimationEventDelegate::onEventCondition(const TimedItem* timedItem, bool isFirstSample, TimedItem::Phase previousPhase, double previousIteration)
{
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
        ASSERT(!std::isnan(timedItem->specifiedTiming().iterationDuration));
        const double elapsedTime = timedItem->specifiedTiming().iterationDuration * (previousIteration + 1);
        maybeDispatch(Document::ANIMATIONITERATION_LISTENER, EventTypeNames::animationiteration, elapsedTime);
        return;
    }
    if ((isFirstSample || previousPhase == TimedItem::PhaseBefore) && isLaterPhase(currentPhase, TimedItem::PhaseBefore)) {
        ASSERT(timedItem->specifiedTiming().startDelay > 0 || isFirstSample);
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
    const TimedItem::Phase currentPhase = timedItem->phase();
    if (currentPhase == TimedItem::PhaseAfter && (isFirstSample || previousPhase != currentPhase) && m_target->document().hasListenerType(Document::TRANSITIONEND_LISTENER)) {
        String propertyName = getPropertyNameString(m_property);
        const Timing& timing = timedItem->specifiedTiming();
        double elapsedTime = timing.iterationDuration;
        const AtomicString& eventType = EventTypeNames::transitionend;
        String pseudoElement = PseudoElement::pseudoElementNameForEvents(m_target->pseudoId());
        RefPtr<TransitionEvent> event = TransitionEvent::create(eventType, propertyName, elapsedTime, pseudoElement);
        event->setTarget(m_target);
        m_target->document().enqueueAnimationFrameEvent(event);
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
    case CSSPropertyFlexBasis:
    case CSSPropertyFlexGrow:
    case CSSPropertyFlexShrink:
    case CSSPropertyFloodColor:
    case CSSPropertyFloodOpacity:
    case CSSPropertyFontSize:
    case CSSPropertyFontWeight:
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
    case CSSPropertyTextDecorationColor:
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
    case CSSPropertyWebkitMaskBoxImageOutset:
    case CSSPropertyWebkitMaskBoxImageSlice:
    case CSSPropertyWebkitMaskBoxImageSource:
    case CSSPropertyWebkitMaskBoxImageWidth:
    case CSSPropertyWebkitMaskImage:
    case CSSPropertyWebkitMaskPositionX:
    case CSSPropertyWebkitMaskPositionY:
    case CSSPropertyWebkitMaskSize:
    case CSSPropertyWebkitPerspective:
    case CSSPropertyWebkitPerspectiveOriginX:
    case CSSPropertyWebkitPerspectiveOriginY:
    case CSSPropertyShapeInside:
    case CSSPropertyShapeOutside:
    case CSSPropertyShapeMargin:
    case CSSPropertyShapeImageThreshold:
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
            if (isAnimatableProperty(id))
                properties.append(id);
        }
        propertyShorthand = StylePropertyShorthand(CSSPropertyInvalid, properties.begin(), properties.size());
    }
    return propertyShorthand;
}

} // namespace WebCore
