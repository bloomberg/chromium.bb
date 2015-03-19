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

#include "core/StylePropertyShorthand.h"
#include "core/animation/AnimationPlayer.h"
#include "core/animation/AnimationTimeline.h"
#include "core/animation/CompositorAnimations.h"
#include "core/animation/DeferredLegacyStyleInterpolation.h"
#include "core/animation/ElementAnimations.h"
#include "core/animation/Interpolation.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/animation/css/CSSAnimatableValueFactory.h"
#include "core/animation/css/CSSPropertyEquality.h"
#include "core/css/CSSKeyframeRule.h"
#include "core/css/CSSPropertyMetadata.h"
#include "core/css/CSSValueList.h"
#include "core/css/resolver/CSSToStyleMap.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Element.h"
#include "core/dom/PseudoElement.h"
#include "core/dom/StyleEngine.h"
#include "core/events/AnimationEvent.h"
#include "core/events/TransitionEvent.h"
#include "core/frame/UseCounter.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/style/KeyframeList.h"
#include "core/paint/DeprecatedPaintLayer.h"
#include "platform/animation/TimingFunction.h"
#include "public/platform/Platform.h"
#include "wtf/BitArray.h"
#include "wtf/HashSet.h"

namespace blink {

namespace {

CSSPropertyID propertyForAnimation(CSSPropertyID property)
{
    switch (property) {
    case CSSPropertyWebkitPerspective:
        return CSSPropertyPerspective;
    case CSSPropertyWebkitTransform:
        return CSSPropertyTransform;
    case CSSPropertyWebkitPerspectiveOriginX:
    case CSSPropertyWebkitPerspectiveOriginY:
    case CSSPropertyWebkitPerspectiveOrigin:
        return CSSPropertyPerspectiveOrigin;
    case CSSPropertyWebkitTransformOriginX:
    case CSSPropertyWebkitTransformOriginY:
    case CSSPropertyWebkitTransformOriginZ:
    case CSSPropertyWebkitTransformOrigin:
        return CSSPropertyTransformOrigin;
    default:
        break;
    }
    return property;
}

static PassRefPtrWillBeRawPtr<StringKeyframeEffectModel> createKeyframeEffect(StyleResolver* resolver, const Element* animatingElement, Element& element, const LayoutStyle* style, const LayoutStyle* parentStyle, const AtomicString& name, TimingFunction* defaultTimingFunction)
{
    // When the animating element is null, use its parent for scoping purposes.
    const Element* elementForScoping = animatingElement ? animatingElement : &element;
    const StyleRuleKeyframes* keyframesRule = resolver->findKeyframesRule(elementForScoping, name);
    ASSERT(keyframesRule);

    StringKeyframeVector keyframes;
    const WillBeHeapVector<RefPtrWillBeMember<StyleRuleKeyframe>>& styleKeyframes = keyframesRule->keyframes();

    // Construct and populate the style for each keyframe
    PropertySet specifiedPropertiesForUseCounter;
    for (size_t i = 0; i < styleKeyframes.size(); ++i) {
        const StyleRuleKeyframe* styleKeyframe = styleKeyframes[i].get();
        RefPtrWillBeRawPtr<StringKeyframe> keyframe = StringKeyframe::create();
        const Vector<double>& offsets = styleKeyframe->keys();
        ASSERT(!offsets.isEmpty());
        keyframe->setOffset(offsets[0]);
        keyframe->setEasing(defaultTimingFunction);
        const StylePropertySet& properties = styleKeyframe->properties();
        for (unsigned j = 0; j < properties.propertyCount(); j++) {
            specifiedPropertiesForUseCounter.add(properties.propertyAt(j).id());
            CSSPropertyID property = propertyForAnimation(properties.propertyAt(j).id());
            if (property == CSSPropertyWebkitAnimationTimingFunction || property == CSSPropertyAnimationTimingFunction) {
                CSSValue* value = properties.propertyAt(j).value();
                RefPtr<TimingFunction> timingFunction;
                if (value->isInheritedValue() && parentStyle->animations()) {
                    timingFunction = parentStyle->animations()->timingFunctionList()[0];
                } else if (value->isValueList()) {
                    timingFunction = CSSToStyleMap::mapAnimationTimingFunction(toCSSValueList(value)->item(0));
                } else {
                    ASSERT(value->isInheritedValue() || value->isInitialValue() || value->isUnsetValue());
                    timingFunction = CSSTimingData::initialTimingFunction();
                }
                keyframe->setEasing(timingFunction.release());
            } else if (CSSPropertyMetadata::isAnimatableProperty(property)) {
                keyframe->setPropertyValue(property, properties.propertyAt(j).value());
            }
        }
        keyframes.append(keyframe);
        // The last keyframe specified at a given offset is used.
        for (size_t j = 1; j < offsets.size(); ++j) {
            keyframes.append(toStringKeyframe(keyframe->cloneWithOffset(offsets[j]).get()));
        }
    }

    for (CSSPropertyID property : specifiedPropertiesForUseCounter) {
        ASSERT(property != CSSPropertyInvalid);
        blink::Platform::current()->histogramSparse("WebCore.Animation.CSSProperties", UseCounter::mapCSSPropertyIdToCSSSampleIdForHistogram(property));
    }

    // Merge duplicate keyframes.
    std::stable_sort(keyframes.begin(), keyframes.end(), Keyframe::compareOffsets);
    size_t targetIndex = 0;
    for (size_t i = 1; i < keyframes.size(); i++) {
        if (keyframes[i]->offset() == keyframes[targetIndex]->offset()) {
            for (CSSPropertyID property : keyframes[i]->properties())
                keyframes[targetIndex]->setPropertyValue(property, keyframes[i]->propertyValue(property));
        } else {
            targetIndex++;
            keyframes[targetIndex] = keyframes[i];
        }
    }
    if (!keyframes.isEmpty())
        keyframes.shrink(targetIndex + 1);

    // Add 0% and 100% keyframes if absent.
    RefPtrWillBeRawPtr<StringKeyframe> startKeyframe = keyframes.isEmpty() ? nullptr : keyframes[0];
    if (!startKeyframe || keyframes[0]->offset() != 0) {
        startKeyframe = StringKeyframe::create();
        startKeyframe->setOffset(0);
        startKeyframe->setEasing(defaultTimingFunction);
        keyframes.prepend(startKeyframe);
    }
    RefPtrWillBeRawPtr<StringKeyframe> endKeyframe = keyframes[keyframes.size() - 1];
    if (endKeyframe->offset() != 1) {
        endKeyframe = StringKeyframe::create();
        endKeyframe->setOffset(1);
        endKeyframe->setEasing(defaultTimingFunction);
        keyframes.append(endKeyframe);
    }
    ASSERT(keyframes.size() >= 2);
    ASSERT(!keyframes.first()->offset());
    ASSERT(keyframes.last()->offset() == 1);

    // FIXME: This is only used for use counting neutral keyframes running on the compositor.
    PropertySet allProperties;
    for (const auto& keyframe : keyframes) {
        for (CSSPropertyID property : keyframe->properties())
            allProperties.add(property);
    }
    const PropertySet& startKeyframeProperties = startKeyframe->properties();
    const PropertySet& endKeyframeProperties = endKeyframe->properties();
    bool missingStartValues = startKeyframeProperties.size() < allProperties.size();
    bool missingEndValues = endKeyframeProperties.size() < allProperties.size();
    if (missingStartValues || missingEndValues) {
        for (CSSPropertyID property : allProperties) {
            bool startNeedsValue = missingStartValues && !startKeyframeProperties.contains(property);
            bool endNeedsValue = missingEndValues && !endKeyframeProperties.contains(property);
            if (!startNeedsValue && !endNeedsValue)
                continue;
            if (CompositorAnimations::isCompositableProperty(property))
                UseCounter::count(elementForScoping->document(), UseCounter::SyntheticKeyframesInCompositedCSSAnimation);
        }
    }

    RefPtrWillBeRawPtr<StringKeyframeEffectModel> effect = StringKeyframeEffectModel::create(keyframes, &keyframes[0]->easing());
    effect->forceConversionsToAnimatableValues(element, style);
    return effect;
}

} // namespace

CSSAnimations::CSSAnimations()
{
}

const AtomicString CSSAnimations::getAnimationNameForInspector(const AnimationPlayer& player)
{
    for (const auto& it : m_animations) {
        if (it.value->player->sequenceNumber() == player.sequenceNumber())
            return it.key;
    }
    return nullAtom;
}

bool CSSAnimations::isTransitionAnimationForInspector(const AnimationPlayer& player) const
{
    for (const auto& it : m_transitions) {
        if (it.value.player->sequenceNumber() == player.sequenceNumber())
            return true;
    }
    return false;
}

PassOwnPtrWillBeRawPtr<CSSAnimationUpdate> CSSAnimations::calculateUpdate(const Element* animatingElement, Element& element, const LayoutStyle& style, LayoutStyle* parentStyle, StyleResolver* resolver)
{
    OwnPtrWillBeRawPtr<CSSAnimationUpdate> update = adoptPtrWillBeNoop(new CSSAnimationUpdate());
    calculateAnimationUpdate(update.get(), animatingElement, element, style, parentStyle, resolver);
    calculateAnimationActiveInterpolations(update.get(), animatingElement, element.document().timeline().currentTimeInternal());
    calculateTransitionUpdate(update.get(), animatingElement, style);
    calculateTransitionActiveInterpolations(update.get(), animatingElement, element.document().timeline().currentTimeInternal());
    return update->isEmpty() ? nullptr : update.release();
}

void CSSAnimations::calculateAnimationUpdate(CSSAnimationUpdate* update, const Element* animatingElement, Element& element, const LayoutStyle& style, LayoutStyle* parentStyle, StyleResolver* resolver)
{
    const ElementAnimations* elementAnimations = animatingElement ? animatingElement->elementAnimations() : nullptr;

    bool isAnimationStyleChange = elementAnimations && elementAnimations->isAnimationStyleChange();

#if !ENABLE(ASSERT)
    // If we're in an animation style change, no animations can have started, been cancelled or changed play state.
    // When ASSERT is enabled, we verify this optimization.
    if (isAnimationStyleChange)
        return;
#endif

    const CSSAnimationData* animationData = style.animations();
    const CSSAnimations* cssAnimations = elementAnimations ? &elementAnimations->cssAnimations() : nullptr;
    const Element* elementForScoping = animatingElement ? animatingElement : &element;

    HashSet<AtomicString> inactive;
    if (cssAnimations) {
        for (const auto& entry : cssAnimations->m_animations)
            inactive.add(entry.key);
    }

    if (style.display() != NONE) {
        for (size_t i = 0; animationData && i < animationData->nameList().size(); ++i) {
            AtomicString animationName(animationData->nameList()[i]);
            if (animationName == CSSAnimationData::initialName())
                continue;

            const bool isPaused = CSSTimingData::getRepeated(animationData->playStateList(), i) == AnimPlayStatePaused;

            Timing timing = animationData->convertToTiming(i);
            Timing specifiedTiming = timing;
            RefPtr<TimingFunction> keyframeTimingFunction = timing.timingFunction;
            timing.timingFunction = Timing::defaults().timingFunction;

            RefPtrWillBeRawPtr<StyleRuleKeyframes> keyframesRule = resolver->findKeyframesRule(elementForScoping, animationName);
            if (!keyframesRule)
                continue; // Cancel the animation if there's no style rule for it.

            if (cssAnimations) {
                AnimationMap::const_iterator existing(cssAnimations->m_animations.find(animationName));
                if (existing != cssAnimations->m_animations.end()) {
                    inactive.remove(animationName);

                    const RunningAnimation* runningAnimation = existing->value.get();
                    AnimationPlayer* player = runningAnimation->player.get();

                    if (keyframesRule != runningAnimation->styleRule || keyframesRule->version() != runningAnimation->styleRuleVersion || runningAnimation->specifiedTiming != specifiedTiming) {
                        ASSERT(!isAnimationStyleChange);
                        update->updateAnimation(animationName, player, InertAnimation::create(
                            createKeyframeEffect(resolver, animatingElement, element, &style, parentStyle, animationName, keyframeTimingFunction.get()),
                            timing, isPaused, player->unlimitedCurrentTimeInternal()), specifiedTiming, keyframesRule);
                    } else if (!isAnimationStyleChange && player->source() && player->source()->isAnimation()) {
                        AnimationEffect* effect = toAnimation(player->source())->effect();
                        if (effect && effect->isKeyframeEffectModel()) {
                            KeyframeEffectModelBase* keyframeEffect = toKeyframeEffectModelBase(effect);
                            if (keyframeEffect->hasSyntheticKeyframes())
                                update->updateAnimationStyle(player, keyframeEffect, animatingElement->layoutObject(), style);
                        }
                    }

                    if (isPaused != player->paused()) {
                        ASSERT(!isAnimationStyleChange);
                        update->toggleAnimationPaused(animationName);
                    }

                    continue;
                }
            }

            ASSERT(!isAnimationStyleChange);
            update->startAnimation(animationName, InertAnimation::create(
                createKeyframeEffect(resolver, animatingElement, element, &style, parentStyle, animationName, keyframeTimingFunction.get()),
                timing, isPaused, 0), specifiedTiming, keyframesRule);
        }
    }

    ASSERT(inactive.isEmpty() || cssAnimations);
    for (const AtomicString& animationName : inactive) {
        ASSERT(!isAnimationStyleChange);
        update->cancelAnimation(animationName, *cssAnimations->m_animations.get(animationName)->player);
    }
}

void CSSAnimations::maybeApplyPendingUpdate(Element* element)
{
    if (!m_pendingUpdate) {
        m_previousActiveInterpolationsForAnimations.clear();
        return;
    }

    OwnPtrWillBeRawPtr<CSSAnimationUpdate> update = m_pendingUpdate.release();

    m_previousActiveInterpolationsForAnimations.swap(update->activeInterpolationsForAnimations());

    // FIXME: cancelling, pausing, unpausing animations all query compositingState, which is not necessarily up to date here
    // since we call this from recalc style.
    // https://code.google.com/p/chromium/issues/detail?id=339847
    DisableCompositingQueryAsserts disabler;

    for (const AtomicString& animationName : update->cancelledAnimationNames()) {
        RefPtrWillBeRawPtr<AnimationPlayer> player = m_animations.take(animationName)->player;
        player->cancel();
        player->update(TimingUpdateOnDemand);
    }

    for (const AtomicString& animationName : update->animationsWithPauseToggled()) {
        AnimationPlayer* player = m_animations.get(animationName)->player.get();
        if (player->paused())
            player->unpause();
        else
            player->pause();
        if (player->outdated())
            player->update(TimingUpdateOnDemand);
    }

    for (const auto& entry : update->animationsWithUpdates()) {
        Animation* animation = toAnimation(entry.player->source());

        animation->setEffect(entry.animation->effect());
        animation->updateSpecifiedTiming(entry.animation->specifiedTiming());

        m_animations.find(entry.name)->value->update(entry);
    }

    for (const auto& styleUpdate : update->animationsWithStyleUpdates()) {
        styleUpdate.effect->forEachInterpolation([](Interpolation& interpolation) {
            if (interpolation.isStyleInterpolation() && toStyleInterpolation(interpolation).isDeferredLegacyStyleInterpolation())
                toDeferredLegacyStyleInterpolation(toStyleInterpolation(interpolation)).underlyingStyleChanged();
        });

        bool updated = false;
        if (styleUpdate.snapshot.opacity)
            updated |= styleUpdate.effect->updateNeutralKeyframeAnimatableValues(CSSPropertyOpacity, styleUpdate.snapshot.opacity);
        if (styleUpdate.snapshot.transform)
            updated |= styleUpdate.effect->updateNeutralKeyframeAnimatableValues(CSSPropertyTransform, styleUpdate.snapshot.transform);
        if (styleUpdate.snapshot.webkitFilter)
            updated |= styleUpdate.effect->updateNeutralKeyframeAnimatableValues(CSSPropertyWebkitFilter, styleUpdate.snapshot.webkitFilter);
        if (updated) {
            styleUpdate.player->setOutdated();
            styleUpdate.player->setCompositorPending(true);
        }
    }

    for (const auto& entry : update->newAnimations()) {
        const InertAnimation* inertAnimation = entry.animation.get();
        OwnPtrWillBeRawPtr<AnimationEventDelegate> eventDelegate = adoptPtrWillBeNoop(new AnimationEventDelegate(element, entry.name));
        RefPtrWillBeRawPtr<Animation> animation = Animation::create(element, inertAnimation->effect(), inertAnimation->specifiedTiming(), Animation::DefaultPriority, eventDelegate.release());
        animation->setName(inertAnimation->name());
        RefPtrWillBeRawPtr<AnimationPlayer> player = element->document().timeline().play(animation.get());
        if (inertAnimation->paused())
            player->pause();
        player->update(TimingUpdateOnDemand);

        m_animations.set(entry.name, adoptRefWillBeNoop(new RunningAnimation(player, entry)));
    }

    // Transitions that are run on the compositor only update main-thread state
    // lazily. However, we need the new state to know what the from state shoud
    // be when transitions are retargeted. Instead of triggering complete style
    // recalculation, we find these cases by searching for new transitions that
    // have matching cancelled animation property IDs on the compositor.
    WillBeHeapHashMap<CSSPropertyID, std::pair<RefPtrWillBeMember<Animation>, double>> retargetedCompositorTransitions;
    for (CSSPropertyID id : update->cancelledTransitions()) {
        ASSERT(m_transitions.contains(id));

        RefPtrWillBeRawPtr<AnimationPlayer> player = m_transitions.take(id).player;
        Animation* animation = toAnimation(player->source());
        if (animation->hasActiveAnimationsOnCompositor(id) && update->newTransitions().find(id) != update->newTransitions().end() && !player->limited())
            retargetedCompositorTransitions.add(id, std::pair<RefPtrWillBeMember<Animation>, double>(animation, player->startTimeInternal()));
        player->cancel();
        player->update(TimingUpdateOnDemand);
    }

    for (CSSPropertyID id : update->finishedTransitions()) {
        // This transition can also be cancelled and finished at the same time
        if (m_transitions.contains(id))
            m_transitions.take(id);
    }

    for (const auto& entry : update->newTransitions()) {
        const CSSAnimationUpdate::NewTransition& newTransition = entry.value;

        RunningTransition runningTransition;
        runningTransition.from = newTransition.from;
        runningTransition.to = newTransition.to;

        CSSPropertyID id = newTransition.id;
        InertAnimation* inertAnimation = newTransition.animation.get();
        OwnPtrWillBeRawPtr<TransitionEventDelegate> eventDelegate = adoptPtrWillBeNoop(new TransitionEventDelegate(element, newTransition.eventId));

        RefPtrWillBeRawPtr<AnimationEffect> effect = inertAnimation->effect();

        if (retargetedCompositorTransitions.contains(id)) {
            const std::pair<RefPtrWillBeMember<Animation>, double>& oldTransition = retargetedCompositorTransitions.get(id);
            RefPtrWillBeRawPtr<Animation> oldAnimation = oldTransition.first;
            double oldStartTime = oldTransition.second;
            double inheritedTime = isNull(oldStartTime) ? 0 : element->document().timeline().currentTimeInternal() - oldStartTime;

            AnimatableValueKeyframeEffectModel* oldEffect = toAnimatableValueKeyframeEffectModel(inertAnimation->effect());
            const KeyframeVector& frames = oldEffect->getFrames();

            AnimatableValueKeyframeVector newFrames;
            newFrames.append(toAnimatableValueKeyframe(frames[0]->clone().get()));
            newFrames.append(toAnimatableValueKeyframe(frames[1]->clone().get()));
            newFrames.append(toAnimatableValueKeyframe(frames[2]->clone().get()));
            newFrames[0]->clearPropertyValue(id);
            newFrames[1]->clearPropertyValue(id);

            RefPtrWillBeRawPtr<InertAnimation> inertAnimationForSampling = InertAnimation::create(oldAnimation->effect(), oldAnimation->specifiedTiming(), false, inheritedTime);
            OwnPtrWillBeRawPtr<WillBeHeapVector<RefPtrWillBeMember<Interpolation>>> sample = nullptr;
            inertAnimationForSampling->sample(sample);
            if (sample && sample->size() == 1) {
                newFrames[0]->setPropertyValue(id, toLegacyStyleInterpolation(sample->at(0).get())->currentValue());
                newFrames[1]->setPropertyValue(id, toLegacyStyleInterpolation(sample->at(0).get())->currentValue());
                effect = AnimatableValueKeyframeEffectModel::create(newFrames);
            }
        }

        RefPtrWillBeRawPtr<Animation> transition = Animation::create(element, effect, inertAnimation->specifiedTiming(), Animation::TransitionPriority, eventDelegate.release());
        transition->setName(inertAnimation->name());
        RefPtrWillBeRawPtr<AnimationPlayer> player = element->document().timeline().play(transition.get());
        player->update(TimingUpdateOnDemand);
        runningTransition.player = player;
        m_transitions.set(id, runningTransition);
        ASSERT(id != CSSPropertyInvalid);
        blink::Platform::current()->histogramSparse("WebCore.Animation.CSSProperties", UseCounter::mapCSSPropertyIdToCSSSampleIdForHistogram(id));
    }
}

void CSSAnimations::calculateTransitionUpdateForProperty(CSSPropertyID id, CSSPropertyID eventId, const CSSTransitionData& transitionData, size_t transitionIndex, const LayoutStyle& oldStyle, const LayoutStyle& style, const TransitionMap* activeTransitions, CSSAnimationUpdate* update, const Element* element)
{
    RefPtrWillBeRawPtr<AnimatableValue> to = nullptr;
    if (activeTransitions) {
        TransitionMap::const_iterator activeTransitionIter = activeTransitions->find(id);
        if (activeTransitionIter != activeTransitions->end()) {
            to = CSSAnimatableValueFactory::create(id, style);
            const AnimatableValue* activeTo = activeTransitionIter->value.to;
            if (to->equals(activeTo))
                return;
            update->cancelTransition(id);
            ASSERT(!element->elementAnimations() || !element->elementAnimations()->isAnimationStyleChange());
        }
    }

    if (CSSPropertyEquality::propertiesEqual(id, oldStyle, style))
        return;
    if (!to)
        to = CSSAnimatableValueFactory::create(id, style);

    RefPtrWillBeRawPtr<AnimatableValue> from = CSSAnimatableValueFactory::create(id, oldStyle);
    // If we have multiple transitions on the same property, we will use the
    // last one since we iterate over them in order.
    if (AnimatableValue::usesDefaultInterpolation(to.get(), from.get()))
        return;

    Timing timing = transitionData.convertToTiming(transitionIndex);
    if (timing.startDelay + timing.iterationDuration <= 0)
        return;

    AnimatableValueKeyframeVector keyframes;
    double startKeyframeOffset = 0;

    if (timing.startDelay > 0) {
        timing.iterationDuration += timing.startDelay;
        startKeyframeOffset = timing.startDelay / timing.iterationDuration;
        timing.startDelay = 0;
    }

    RefPtrWillBeRawPtr<AnimatableValueKeyframe> delayKeyframe = AnimatableValueKeyframe::create();
    delayKeyframe->setPropertyValue(id, from.get());
    delayKeyframe->setOffset(0);
    keyframes.append(delayKeyframe);

    RefPtrWillBeRawPtr<AnimatableValueKeyframe> startKeyframe = AnimatableValueKeyframe::create();
    startKeyframe->setPropertyValue(id, from.get());
    startKeyframe->setOffset(startKeyframeOffset);
    startKeyframe->setEasing(timing.timingFunction.release());
    timing.timingFunction = LinearTimingFunction::shared();
    keyframes.append(startKeyframe);

    RefPtrWillBeRawPtr<AnimatableValueKeyframe> endKeyframe = AnimatableValueKeyframe::create();
    endKeyframe->setPropertyValue(id, to.get());
    endKeyframe->setOffset(1);
    keyframes.append(endKeyframe);

    RefPtrWillBeRawPtr<AnimatableValueKeyframeEffectModel> effect = AnimatableValueKeyframeEffectModel::create(keyframes);
    update->startTransition(id, eventId, from.get(), to.get(), InertAnimation::create(effect, timing, false, 0));
    ASSERT(!element->elementAnimations() || !element->elementAnimations()->isAnimationStyleChange());
}

void CSSAnimations::calculateTransitionUpdate(CSSAnimationUpdate* update, const Element* animatingElement, const LayoutStyle& style)
{
    if (!animatingElement)
        return;

    ElementAnimations* elementAnimations = animatingElement->elementAnimations();
    const TransitionMap* activeTransitions = elementAnimations ? &elementAnimations->cssAnimations().m_transitions : nullptr;
    const CSSTransitionData* transitionData = style.transitions();

#if ENABLE(ASSERT)
    // In debug builds we verify that it would have been safe to avoid populating and testing listedProperties if the style recalc is due to animation.
    const bool animationStyleRecalc = false;
#else
    // In release builds we avoid the cost of checking for new and interrupted transitions if the style recalc is due to animation.
    const bool animationStyleRecalc = elementAnimations && elementAnimations->isAnimationStyleChange();
#endif

    BitArray<numCSSProperties> listedProperties;
    bool anyTransitionHadTransitionAll = false;
    const LayoutObject* renderer = animatingElement->layoutObject();
    if (!animationStyleRecalc && style.display() != NONE && renderer && renderer->style() && transitionData) {
        const LayoutStyle& oldStyle = *renderer->style();

        for (size_t i = 0; i < transitionData->propertyList().size(); ++i) {
            const CSSTransitionData::TransitionProperty& transitionProperty = transitionData->propertyList()[i];
            CSSTransitionData::TransitionPropertyType mode = transitionProperty.propertyType;
            CSSPropertyID property = transitionProperty.propertyId;
            if (mode == CSSTransitionData::TransitionNone || mode == CSSTransitionData::TransitionUnknown)
                continue;

            bool animateAll = mode == CSSTransitionData::TransitionAll;
            ASSERT(animateAll || mode == CSSTransitionData::TransitionSingleProperty);
            if (animateAll)
                anyTransitionHadTransitionAll = true;
            const StylePropertyShorthand& propertyList = animateAll ? CSSAnimations::animatableProperties() : shorthandForProperty(property);
            // If not a shorthand we only execute one iteration of this loop, and refer to the property directly.
            for (unsigned j = 0; !j || j < propertyList.length(); ++j) {
                CSSPropertyID id = propertyList.length() ? propertyList.properties()[j] : property;
                CSSPropertyID eventId = id;

                if (!animateAll) {
                    id = propertyForAnimation(id);
                    if (CSSPropertyMetadata::isAnimatableProperty(id))
                        listedProperties.set(id);
                    else
                        continue;
                }

                // FIXME: We should transition if an !important property changes even when an animation is running,
                // but this is a bit hard to do with the current applyMatchedProperties system.
                if (!update->activeInterpolationsForAnimations().contains(id)
                    && (!elementAnimations || !elementAnimations->cssAnimations().m_previousActiveInterpolationsForAnimations.contains(id))) {
                    calculateTransitionUpdateForProperty(id, eventId, *transitionData, i, oldStyle, style, activeTransitions, update, animatingElement);
                }
            }
        }
    }

    if (activeTransitions) {
        for (const auto& entry : *activeTransitions) {
            CSSPropertyID id = entry.key;
            if (!anyTransitionHadTransitionAll && !animationStyleRecalc && !listedProperties.get(id)) {
                // TODO: Figure out why this fails on Chrome OS login page. crbug.com/365507
                // ASSERT(player.playStateInternal() == AnimationPlayer::Finished || !(elementAnimations && elementAnimations->isAnimationStyleChange()));
                update->cancelTransition(id);
            } else if (entry.value.player->finishedInternal()) {
                update->finishTransition(id);
            }
        }
    }
}

void CSSAnimations::cancel()
{
    for (const auto& entry : m_animations) {
        entry.value->player->cancel();
        entry.value->player->update(TimingUpdateOnDemand);
    }

    for (const auto& entry : m_transitions) {
        entry.value.player->cancel();
        entry.value.player->update(TimingUpdateOnDemand);
    }

    m_animations.clear();
    m_transitions.clear();
    m_pendingUpdate = nullptr;
}

void CSSAnimations::calculateAnimationActiveInterpolations(CSSAnimationUpdate* update, const Element* animatingElement, double timelineCurrentTime)
{
    ElementAnimations* elementAnimations = animatingElement ? animatingElement->elementAnimations() : nullptr;
    AnimationStack* animationStack = elementAnimations ? &elementAnimations->defaultStack() : nullptr;

    if (update->newAnimations().isEmpty() && update->suppressedAnimationAnimationPlayers().isEmpty()) {
        WillBeHeapHashMap<CSSPropertyID, RefPtrWillBeMember<Interpolation>> activeInterpolationsForAnimations(AnimationStack::activeInterpolations(animationStack, 0, 0, Animation::DefaultPriority, timelineCurrentTime));
        update->adoptActiveInterpolationsForAnimations(activeInterpolationsForAnimations);
        return;
    }

    WillBeHeapVector<RawPtrWillBeMember<InertAnimation>> newAnimations;
    for (const auto& newAnimation : update->newAnimations())
        newAnimations.append(newAnimation.animation.get());
    for (const auto& updatedAnimation : update->animationsWithUpdates())
        newAnimations.append(updatedAnimation.animation.get()); // Animations with updates use a temporary InertAnimation for the current frame.

    WillBeHeapHashMap<CSSPropertyID, RefPtrWillBeMember<Interpolation>> activeInterpolationsForAnimations(AnimationStack::activeInterpolations(animationStack, &newAnimations, &update->suppressedAnimationAnimationPlayers(), Animation::DefaultPriority, timelineCurrentTime));
    update->adoptActiveInterpolationsForAnimations(activeInterpolationsForAnimations);
}

void CSSAnimations::calculateTransitionActiveInterpolations(CSSAnimationUpdate* update, const Element* animatingElement, double timelineCurrentTime)
{
    ElementAnimations* elementAnimations = animatingElement ? animatingElement->elementAnimations() : nullptr;
    AnimationStack* animationStack = elementAnimations ? &elementAnimations->defaultStack() : nullptr;

    WillBeHeapHashMap<CSSPropertyID, RefPtrWillBeMember<Interpolation>> activeInterpolationsForTransitions;
    if (update->newTransitions().isEmpty() && update->cancelledTransitions().isEmpty()) {
        activeInterpolationsForTransitions = AnimationStack::activeInterpolations(animationStack, 0, 0, Animation::TransitionPriority, timelineCurrentTime);
    } else {
        WillBeHeapVector<RawPtrWillBeMember<InertAnimation>> newTransitions;
        for (const auto& entry : update->newTransitions())
            newTransitions.append(entry.value.animation.get());

        WillBeHeapHashSet<RawPtrWillBeMember<const AnimationPlayer>> cancelledAnimationPlayers;
        if (!update->cancelledTransitions().isEmpty()) {
            ASSERT(elementAnimations);
            const TransitionMap& transitionMap = elementAnimations->cssAnimations().m_transitions;
            for (CSSPropertyID id : update->cancelledTransitions()) {
                ASSERT(transitionMap.contains(id));
                cancelledAnimationPlayers.add(transitionMap.get(id).player.get());
            }
        }

        activeInterpolationsForTransitions = AnimationStack::activeInterpolations(animationStack, &newTransitions, &cancelledAnimationPlayers, Animation::TransitionPriority, timelineCurrentTime);
    }

    // Properties being animated by animations don't get values from transitions applied.
    if (!update->activeInterpolationsForAnimations().isEmpty() && !activeInterpolationsForTransitions.isEmpty()) {
        for (const auto& entry : update->activeInterpolationsForAnimations())
            activeInterpolationsForTransitions.remove(entry.key);
    }
    update->adoptActiveInterpolationsForTransitions(activeInterpolationsForTransitions);
}

EventTarget* CSSAnimations::AnimationEventDelegate::eventTarget() const
{
    return EventPath::eventTargetRespectingTargetRules(*m_animationTarget);
}

void CSSAnimations::AnimationEventDelegate::maybeDispatch(Document::ListenerType listenerType, const AtomicString& eventName, double elapsedTime)
{
    if (m_animationTarget->document().hasListenerType(listenerType)) {
        RefPtrWillBeRawPtr<AnimationEvent> event = AnimationEvent::create(eventName, m_name, elapsedTime);
        event->setTarget(eventTarget());
        document().enqueueAnimationFrameEvent(event);
    }
}

bool CSSAnimations::AnimationEventDelegate::requiresIterationEvents(const AnimationNode& animationNode)
{
    return document().hasListenerType(Document::ANIMATIONITERATION_LISTENER);
}

void CSSAnimations::AnimationEventDelegate::onEventCondition(const AnimationNode& animationNode)
{
    const AnimationNode::Phase currentPhase = animationNode.phase();
    const double currentIteration = animationNode.currentIteration();

    if (m_previousPhase != currentPhase
        && (currentPhase == AnimationNode::PhaseActive || currentPhase == AnimationNode::PhaseAfter)
        && (m_previousPhase == AnimationNode::PhaseNone || m_previousPhase == AnimationNode::PhaseBefore)) {
        const double startDelay = animationNode.specifiedTiming().startDelay;
        const double elapsedTime = startDelay < 0 ? -startDelay : 0;
        maybeDispatch(Document::ANIMATIONSTART_LISTENER, EventTypeNames::animationstart, elapsedTime);
    }

    if (currentPhase == AnimationNode::PhaseActive && m_previousPhase == currentPhase && m_previousIteration != currentIteration) {
        // We fire only a single event for all iterations thast terminate
        // between a single pair of samples. See http://crbug.com/275263. For
        // compatibility with the existing implementation, this event uses
        // the elapsedTime for the first iteration in question.
        ASSERT(!std::isnan(animationNode.specifiedTiming().iterationDuration));
        const double elapsedTime = animationNode.specifiedTiming().iterationDuration * (m_previousIteration + 1);
        maybeDispatch(Document::ANIMATIONITERATION_LISTENER, EventTypeNames::animationiteration, elapsedTime);
    }

    if (currentPhase == AnimationNode::PhaseAfter && m_previousPhase != AnimationNode::PhaseAfter)
        maybeDispatch(Document::ANIMATIONEND_LISTENER, EventTypeNames::animationend, animationNode.activeDurationInternal());

    m_previousPhase = currentPhase;
    m_previousIteration = currentIteration;
}

DEFINE_TRACE(CSSAnimations::AnimationEventDelegate)
{
    visitor->trace(m_animationTarget);
    AnimationNode::EventDelegate::trace(visitor);
}

EventTarget* CSSAnimations::TransitionEventDelegate::eventTarget() const
{
    return EventPath::eventTargetRespectingTargetRules(*m_transitionTarget);
}

void CSSAnimations::TransitionEventDelegate::onEventCondition(const AnimationNode& animationNode)
{
    const AnimationNode::Phase currentPhase = animationNode.phase();
    if (currentPhase == AnimationNode::PhaseAfter && currentPhase != m_previousPhase && document().hasListenerType(Document::TRANSITIONEND_LISTENER)) {
        String propertyName = getPropertyNameString(m_property);
        const Timing& timing = animationNode.specifiedTiming();
        double elapsedTime = timing.iterationDuration;
        const AtomicString& eventType = EventTypeNames::transitionend;
        String pseudoElement = PseudoElement::pseudoElementNameForEvents(pseudoId());
        RefPtrWillBeRawPtr<TransitionEvent> event = TransitionEvent::create(eventType, propertyName, elapsedTime, pseudoElement);
        event->setTarget(eventTarget());
        document().enqueueAnimationFrameEvent(event);
    }

    m_previousPhase = currentPhase;
}

DEFINE_TRACE(CSSAnimations::TransitionEventDelegate)
{
    visitor->trace(m_transitionTarget);
    AnimationNode::EventDelegate::trace(visitor);
}

const StylePropertyShorthand& CSSAnimations::animatableProperties()
{
    DEFINE_STATIC_LOCAL(Vector<CSSPropertyID>, properties, ());
    DEFINE_STATIC_LOCAL(StylePropertyShorthand, propertyShorthand, ());
    if (properties.isEmpty()) {
        for (int i = firstCSSProperty; i < lastCSSProperty; ++i) {
            CSSPropertyID id = convertToCSSPropertyID(i);
            if (CSSPropertyMetadata::isAnimatableProperty(id))
                properties.append(id);
        }
        propertyShorthand = StylePropertyShorthand(CSSPropertyInvalid, properties.begin(), properties.size());
    }
    return propertyShorthand;
}

// Animation properties are not allowed to be affected by Web Animations.
// http://dev.w3.org/fxtf/web-animations/#not-animatable
bool CSSAnimations::isAllowedAnimation(CSSPropertyID property)
{
    switch (property) {
    case CSSPropertyAnimation:
    case CSSPropertyAnimationDelay:
    case CSSPropertyAnimationDirection:
    case CSSPropertyAnimationDuration:
    case CSSPropertyAnimationFillMode:
    case CSSPropertyAnimationIterationCount:
    case CSSPropertyAnimationName:
    case CSSPropertyAnimationPlayState:
    case CSSPropertyAnimationTimingFunction:
    case CSSPropertyDisplay:
    case CSSPropertyTransition:
    case CSSPropertyTransitionDelay:
    case CSSPropertyTransitionDuration:
    case CSSPropertyTransitionProperty:
    case CSSPropertyTransitionTimingFunction:
    case CSSPropertyWebkitAnimation:
    case CSSPropertyWebkitAnimationDelay:
    case CSSPropertyWebkitAnimationDirection:
    case CSSPropertyWebkitAnimationDuration:
    case CSSPropertyWebkitAnimationFillMode:
    case CSSPropertyWebkitAnimationIterationCount:
    case CSSPropertyWebkitAnimationName:
    case CSSPropertyWebkitAnimationPlayState:
    case CSSPropertyWebkitAnimationTimingFunction:
    case CSSPropertyWebkitTransition:
    case CSSPropertyWebkitTransitionDelay:
    case CSSPropertyWebkitTransitionDuration:
    case CSSPropertyWebkitTransitionProperty:
    case CSSPropertyWebkitTransitionTimingFunction:
        return false;
    default:
        return true;
    }
}

DEFINE_TRACE(CSSAnimations)
{
#if ENABLE(OILPAN)
    visitor->trace(m_transitions);
    visitor->trace(m_pendingUpdate);
    visitor->trace(m_animations);
    visitor->trace(m_previousActiveInterpolationsForAnimations);
#endif
}

DEFINE_TRACE(CSSAnimationUpdate)
{
#if ENABLE(OILPAN)
    visitor->trace(m_newTransitions);
    visitor->trace(m_activeInterpolationsForAnimations);
    visitor->trace(m_activeInterpolationsForTransitions);
    visitor->trace(m_newAnimations);
    visitor->trace(m_suppressedAnimationPlayers);
    visitor->trace(m_animationsWithUpdates);
    visitor->trace(m_animationsWithStyleUpdates);
#endif
}

} // namespace blink
