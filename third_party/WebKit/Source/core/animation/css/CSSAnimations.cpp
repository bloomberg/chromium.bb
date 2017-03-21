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

#include "core/animation/css/CSSAnimations.h"

#include <algorithm>
#include <bitset>
#include "core/StylePropertyShorthand.h"
#include "core/animation/Animation.h"
#include "core/animation/CSSInterpolationTypesMap.h"
#include "core/animation/CompositorAnimations.h"
#include "core/animation/DocumentTimeline.h"
#include "core/animation/ElementAnimations.h"
#include "core/animation/InertEffect.h"
#include "core/animation/Interpolation.h"
#include "core/animation/InterpolationEnvironment.h"
#include "core/animation/InterpolationType.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/animation/KeyframeEffectReadOnly.h"
#include "core/animation/TransitionInterpolation.h"
#include "core/animation/css/CSSAnimatableValueFactory.h"
#include "core/css/CSSKeyframeRule.h"
#include "core/css/CSSPropertyEquality.h"
#include "core/css/CSSPropertyMetadata.h"
#include "core/css/CSSValueList.h"
#include "core/css/PropertyRegistry.h"
#include "core/css/parser/CSSVariableParser.h"
#include "core/css/resolver/CSSToStyleMap.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Element.h"
#include "core/dom/PseudoElement.h"
#include "core/dom/StyleEngine.h"
#include "core/events/AnimationEvent.h"
#include "core/events/TransitionEvent.h"
#include "core/frame/UseCounter.h"
#include "core/layout/LayoutObject.h"
#include "core/paint/PaintLayer.h"
#include "platform/Histogram.h"
#include "platform/animation/TimingFunction.h"
#include "public/platform/Platform.h"
#include "wtf/HashSet.h"

namespace blink {

using PropertySet = HashSet<CSSPropertyID>;

namespace {

static StringKeyframeEffectModel* createKeyframeEffectModel(
    StyleResolver* resolver,
    const Element* animatingElement,
    Element& element,
    const ComputedStyle* style,
    const ComputedStyle* parentStyle,
    const AtomicString& name,
    TimingFunction* defaultTimingFunction,
    size_t animationIndex) {
  // When the animating element is null, use its parent for scoping purposes.
  const Element* elementForScoping =
      animatingElement ? animatingElement : &element;
  const StyleRuleKeyframes* keyframesRule =
      resolver->findKeyframesRule(elementForScoping, name);
  DCHECK(keyframesRule);

  StringKeyframeVector keyframes;
  const HeapVector<Member<StyleRuleKeyframe>>& styleKeyframes =
      keyframesRule->keyframes();

  // Construct and populate the style for each keyframe
  PropertySet specifiedPropertiesForUseCounter;
  for (size_t i = 0; i < styleKeyframes.size(); ++i) {
    const StyleRuleKeyframe* styleKeyframe = styleKeyframes[i].get();
    RefPtr<StringKeyframe> keyframe = StringKeyframe::create();
    const Vector<double>& offsets = styleKeyframe->keys();
    DCHECK(!offsets.isEmpty());
    keyframe->setOffset(offsets[0]);
    keyframe->setEasing(defaultTimingFunction);
    const StylePropertySet& properties = styleKeyframe->properties();
    for (unsigned j = 0; j < properties.propertyCount(); j++) {
      CSSPropertyID property = properties.propertyAt(j).id();
      specifiedPropertiesForUseCounter.insert(property);
      if (property == CSSPropertyAnimationTimingFunction) {
        const CSSValue& value = properties.propertyAt(j).value();
        RefPtr<TimingFunction> timingFunction;
        if (value.isInheritedValue() && parentStyle->animations()) {
          timingFunction = parentStyle->animations()->timingFunctionList()[0];
        } else if (value.isValueList()) {
          timingFunction = CSSToStyleMap::mapAnimationTimingFunction(
              toCSSValueList(value).item(0));
        } else {
          DCHECK(value.isCSSWideKeyword());
          timingFunction = CSSTimingData::initialTimingFunction();
        }
        keyframe->setEasing(std::move(timingFunction));
      } else if (!CSSAnimations::isAnimationAffectingProperty(property)) {
        keyframe->setCSSPropertyValue(property,
                                      properties.propertyAt(j).value());
      }
    }
    keyframes.push_back(keyframe);
    // The last keyframe specified at a given offset is used.
    for (size_t j = 1; j < offsets.size(); ++j) {
      keyframes.push_back(
          toStringKeyframe(keyframe->cloneWithOffset(offsets[j]).get()));
    }
  }

  DEFINE_STATIC_LOCAL(SparseHistogram, propertyHistogram,
                      ("WebCore.Animation.CSSProperties"));
  for (CSSPropertyID property : specifiedPropertiesForUseCounter) {
    DCHECK(isValidCSSPropertyID(property));
    UseCounter::countAnimatedCSS(elementForScoping->document(), property);

    // TODO(crbug.com/458925): Remove legacy histogram and counts
    propertyHistogram.sample(
        UseCounter::mapCSSPropertyIdToCSSSampleIdForHistogram(property));
  }

  // Merge duplicate keyframes.
  std::stable_sort(keyframes.begin(), keyframes.end(),
                   Keyframe::compareOffsets);
  size_t targetIndex = 0;
  for (size_t i = 1; i < keyframes.size(); i++) {
    if (keyframes[i]->offset() == keyframes[targetIndex]->offset()) {
      for (const auto& property : keyframes[i]->properties())
        keyframes[targetIndex]->setCSSPropertyValue(
            property.cssProperty(), keyframes[i]->cssPropertyValue(property));
    } else {
      targetIndex++;
      keyframes[targetIndex] = keyframes[i];
    }
  }
  if (!keyframes.isEmpty())
    keyframes.shrink(targetIndex + 1);

  // Add 0% and 100% keyframes if absent.
  RefPtr<StringKeyframe> startKeyframe =
      keyframes.isEmpty() ? nullptr : keyframes[0];
  if (!startKeyframe || keyframes[0]->offset() != 0) {
    startKeyframe = StringKeyframe::create();
    startKeyframe->setOffset(0);
    startKeyframe->setEasing(defaultTimingFunction);
    keyframes.push_front(startKeyframe);
  }
  RefPtr<StringKeyframe> endKeyframe = keyframes[keyframes.size() - 1];
  if (endKeyframe->offset() != 1) {
    endKeyframe = StringKeyframe::create();
    endKeyframe->setOffset(1);
    endKeyframe->setEasing(defaultTimingFunction);
    keyframes.push_back(endKeyframe);
  }
  DCHECK_GE(keyframes.size(), 2U);
  DCHECK(!keyframes.front()->offset());
  DCHECK_EQ(keyframes.back()->offset(), 1);

  StringKeyframeEffectModel* model =
      StringKeyframeEffectModel::create(keyframes, &keyframes[0]->easing());
  if (animationIndex > 0 && model->hasSyntheticKeyframes())
    UseCounter::count(elementForScoping->document(),
                      UseCounter::CSSAnimationsStackedNeutralKeyframe);
  return model;
}

}  // namespace

CSSAnimations::CSSAnimations() {}

bool CSSAnimations::isAnimationForInspector(const Animation& animation) {
  for (const auto& runningAnimation : m_runningAnimations) {
    if (runningAnimation->animation->sequenceNumber() ==
        animation.sequenceNumber())
      return true;
  }
  return false;
}

bool CSSAnimations::isTransitionAnimationForInspector(
    const Animation& animation) const {
  for (const auto& it : m_transitions) {
    if (it.value.animation->sequenceNumber() == animation.sequenceNumber())
      return true;
  }
  return false;
}

static const KeyframeEffectModelBase* getKeyframeEffectModelBase(
    const AnimationEffectReadOnly* effect) {
  if (!effect)
    return nullptr;
  const EffectModel* model = nullptr;
  if (effect->isKeyframeEffectReadOnly())
    model = toKeyframeEffectReadOnly(effect)->model();
  else if (effect->isInertEffect())
    model = toInertEffect(effect)->model();
  if (!model || !model->isKeyframeEffectModel())
    return nullptr;
  return toKeyframeEffectModelBase(model);
}

void CSSAnimations::calculateCompositorAnimationUpdate(
    CSSAnimationUpdate& update,
    const Element* animatingElement,
    Element& element,
    const ComputedStyle& style,
    const ComputedStyle* parentStyle,
    bool wasViewportResized) {
  ElementAnimations* elementAnimations =
      animatingElement ? animatingElement->elementAnimations() : nullptr;

  // We only update compositor animations in response to changes in the base
  // style.
  if (!elementAnimations || elementAnimations->isAnimationStyleChange())
    return;

  if (!animatingElement->layoutObject() ||
      !animatingElement->layoutObject()->style())
    return;

  const ComputedStyle& oldStyle = *animatingElement->layoutObject()->style();
  if (!oldStyle.shouldCompositeForCurrentAnimations())
    return;

  bool transformZoomChanged = oldStyle.hasCurrentTransformAnimation() &&
                              oldStyle.effectiveZoom() != style.effectiveZoom();
  for (auto& entry : elementAnimations->animations()) {
    Animation& animation = *entry.key;
    const KeyframeEffectModelBase* keyframeEffect =
        getKeyframeEffectModelBase(animation.effect());
    if (!keyframeEffect)
      continue;

    bool updateCompositorKeyframes = false;
    if ((transformZoomChanged || wasViewportResized) &&
        keyframeEffect->affects(PropertyHandle(CSSPropertyTransform)) &&
        keyframeEffect->snapshotAllCompositorKeyframes(element, style,
                                                       parentStyle)) {
      updateCompositorKeyframes = true;
    } else if (keyframeEffect->hasSyntheticKeyframes() &&
               keyframeEffect->snapshotNeutralCompositorKeyframes(
                   element, oldStyle, style, parentStyle)) {
      updateCompositorKeyframes = true;
    }

    if (updateCompositorKeyframes)
      update.updateCompositorKeyframes(&animation);
  }
}

void CSSAnimations::calculateAnimationUpdate(CSSAnimationUpdate& update,
                                             const Element* animatingElement,
                                             Element& element,
                                             const ComputedStyle& style,
                                             ComputedStyle* parentStyle,
                                             StyleResolver* resolver) {
  const ElementAnimations* elementAnimations =
      animatingElement ? animatingElement->elementAnimations() : nullptr;

  bool isAnimationStyleChange =
      elementAnimations && elementAnimations->isAnimationStyleChange();

#if !DCHECK_IS_ON()
  // If we're in an animation style change, no animations can have started, been
  // cancelled or changed play state. When DCHECK is enabled, we verify this
  // optimization.
  if (isAnimationStyleChange) {
    calculateAnimationActiveInterpolations(update, animatingElement);
    return;
  }
#endif

  const CSSAnimationData* animationData = style.animations();
  const CSSAnimations* cssAnimations =
      elementAnimations ? &elementAnimations->cssAnimations() : nullptr;
  const Element* elementForScoping =
      animatingElement ? animatingElement : &element;

  Vector<bool> cancelRunningAnimationFlags(
      cssAnimations ? cssAnimations->m_runningAnimations.size() : 0);
  for (bool& flag : cancelRunningAnimationFlags)
    flag = true;

  if (animationData && style.display() != EDisplay::kNone) {
    const Vector<AtomicString>& nameList = animationData->nameList();
    for (size_t i = 0; i < nameList.size(); ++i) {
      AtomicString name = nameList[i];
      if (name == CSSAnimationData::initialName())
        continue;

      // Find n where this is the nth occurence of this animation name.
      size_t nameIndex = 0;
      for (size_t j = 0; j < i; j++) {
        if (nameList[j] == name)
          nameIndex++;
      }

      const bool isPaused =
          CSSTimingData::getRepeated(animationData->playStateList(), i) ==
          AnimPlayStatePaused;

      Timing timing = animationData->convertToTiming(i);
      Timing specifiedTiming = timing;
      RefPtr<TimingFunction> keyframeTimingFunction = timing.timingFunction;
      timing.timingFunction = Timing::defaults().timingFunction;

      StyleRuleKeyframes* keyframesRule =
          resolver->findKeyframesRule(elementForScoping, name);
      if (!keyframesRule)
        continue;  // Cancel the animation if there's no style rule for it.

      const RunningAnimation* existingAnimation = nullptr;
      size_t existingAnimationIndex = 0;

      if (cssAnimations) {
        for (size_t i = 0; i < cssAnimations->m_runningAnimations.size(); i++) {
          const RunningAnimation& runningAnimation =
              *cssAnimations->m_runningAnimations[i];
          if (runningAnimation.name == name &&
              runningAnimation.nameIndex == nameIndex) {
            existingAnimation = &runningAnimation;
            existingAnimationIndex = i;
            break;
          }
        }
      }

      if (existingAnimation) {
        cancelRunningAnimationFlags[existingAnimationIndex] = false;

        Animation* animation = existingAnimation->animation.get();

        if (keyframesRule != existingAnimation->styleRule ||
            keyframesRule->version() != existingAnimation->styleRuleVersion ||
            existingAnimation->specifiedTiming != specifiedTiming) {
          DCHECK(!isAnimationStyleChange);
          update.updateAnimation(
              existingAnimationIndex, animation,
              *InertEffect::create(
                  createKeyframeEffectModel(resolver, animatingElement, element,
                                            &style, parentStyle, name,
                                            keyframeTimingFunction.get(), i),
                  timing, isPaused, animation->unlimitedCurrentTimeInternal()),
              specifiedTiming, keyframesRule);
        }

        if (isPaused != animation->paused()) {
          DCHECK(!isAnimationStyleChange);
          update.toggleAnimationIndexPaused(existingAnimationIndex);
        }
      } else {
        DCHECK(!isAnimationStyleChange);
        update.startAnimation(
            name, nameIndex,
            *InertEffect::create(
                createKeyframeEffectModel(resolver, animatingElement, element,
                                          &style, parentStyle, name,
                                          keyframeTimingFunction.get(), i),
                timing, isPaused, 0),
            specifiedTiming, keyframesRule);
      }
    }
  }

  for (size_t i = 0; i < cancelRunningAnimationFlags.size(); i++) {
    if (cancelRunningAnimationFlags[i]) {
      DCHECK(cssAnimations && !isAnimationStyleChange);
      update.cancelAnimation(i,
                             *cssAnimations->m_runningAnimations[i]->animation);
    }
  }

  calculateAnimationActiveInterpolations(update, animatingElement);
}

void CSSAnimations::snapshotCompositorKeyframes(
    Element& element,
    CSSAnimationUpdate& update,
    const ComputedStyle& style,
    const ComputedStyle* parentStyle) {
  const auto& snapshot = [&element, &style,
                          parentStyle](const AnimationEffectReadOnly* effect) {
    const KeyframeEffectModelBase* keyframeEffect =
        getKeyframeEffectModelBase(effect);
    if (keyframeEffect && keyframeEffect->needsCompositorKeyframesSnapshot())
      keyframeEffect->snapshotAllCompositorKeyframes(element, style,
                                                     parentStyle);
  };

  ElementAnimations* elementAnimations = element.elementAnimations();
  if (elementAnimations) {
    for (auto& entry : elementAnimations->animations())
      snapshot(entry.key->effect());
  }

  for (const auto& newAnimation : update.newAnimations())
    snapshot(newAnimation.effect.get());

  for (const auto& updatedAnimation : update.animationsWithUpdates())
    snapshot(updatedAnimation.effect.get());

  for (const auto& newTransition : update.newTransitions())
    snapshot(newTransition.value.effect.get());
}

void CSSAnimations::maybeApplyPendingUpdate(Element* element) {
  m_previousActiveInterpolationsForAnimations.clear();
  if (m_pendingUpdate.isEmpty())
    return;

  m_previousActiveInterpolationsForAnimations.swap(
      m_pendingUpdate.activeInterpolationsForAnimations());

  // FIXME: cancelling, pausing, unpausing animations all query
  // compositingState, which is not necessarily up to date here
  // since we call this from recalc style.
  // https://code.google.com/p/chromium/issues/detail?id=339847
  DisableCompositingQueryAsserts disabler;

  for (size_t pausedIndex :
       m_pendingUpdate.animationIndicesWithPauseToggled()) {
    Animation& animation = *m_runningAnimations[pausedIndex]->animation;
    if (animation.paused())
      animation.unpause();
    else
      animation.pause();
    if (animation.outdated())
      animation.update(TimingUpdateOnDemand);
  }

  for (const auto& animation : m_pendingUpdate.updatedCompositorKeyframes())
    animation->setCompositorPending(true);

  for (const auto& entry : m_pendingUpdate.animationsWithUpdates()) {
    KeyframeEffectReadOnly* effect =
        toKeyframeEffectReadOnly(entry.animation->effect());

    effect->setModel(entry.effect->model());
    effect->updateSpecifiedTiming(entry.effect->specifiedTiming());

    m_runningAnimations[entry.index]->update(entry);
  }

  const Vector<size_t>& cancelledIndices =
      m_pendingUpdate.cancelledAnimationIndices();
  for (size_t i = cancelledIndices.size(); i-- > 0;) {
    DCHECK(i == cancelledIndices.size() - 1 ||
           cancelledIndices[i] < cancelledIndices[i + 1]);
    Animation& animation = *m_runningAnimations[cancelledIndices[i]]->animation;
    animation.cancel();
    animation.update(TimingUpdateOnDemand);
    m_runningAnimations.remove(cancelledIndices[i]);
  }

  for (const auto& entry : m_pendingUpdate.newAnimations()) {
    const InertEffect* inertAnimation = entry.effect.get();
    AnimationEventDelegate* eventDelegate =
        new AnimationEventDelegate(element, entry.name);
    KeyframeEffect* effect = KeyframeEffect::create(
        element, inertAnimation->model(), inertAnimation->specifiedTiming(),
        KeyframeEffectReadOnly::DefaultPriority, eventDelegate);
    Animation* animation = element->document().timeline().play(effect);
    animation->setId(entry.name);
    if (inertAnimation->paused())
      animation->pause();
    animation->update(TimingUpdateOnDemand);

    m_runningAnimations.push_back(new RunningAnimation(animation, entry));
  }

  // Transitions that are run on the compositor only update main-thread state
  // lazily. However, we need the new state to know what the from state shoud
  // be when transitions are retargeted. Instead of triggering complete style
  // recalculation, we find these cases by searching for new transitions that
  // have matching cancelled animation property IDs on the compositor.
  HeapHashMap<PropertyHandle, std::pair<Member<KeyframeEffectReadOnly>, double>>
      retargetedCompositorTransitions;
  for (const PropertyHandle& property :
       m_pendingUpdate.cancelledTransitions()) {
    DCHECK(m_transitions.contains(property));

    Animation* animation = m_transitions.take(property).animation;
    KeyframeEffectReadOnly* effect =
        toKeyframeEffectReadOnly(animation->effect());
    if (effect->hasActiveAnimationsOnCompositor(property) &&
        m_pendingUpdate.newTransitions().find(property) !=
            m_pendingUpdate.newTransitions().end() &&
        !animation->limited()) {
      retargetedCompositorTransitions.insert(
          property, std::pair<KeyframeEffectReadOnly*, double>(
                        effect, animation->startTimeInternal()));
    }
    animation->cancel();
    // after cancelation, transitions must be downgraded or they'll fail
    // to be considered when retriggering themselves. This can happen if
    // the transition is captured through getAnimations then played.
    if (animation->effect() && animation->effect()->isKeyframeEffectReadOnly())
      toKeyframeEffectReadOnly(animation->effect())->downgradeToNormal();
    animation->update(TimingUpdateOnDemand);
  }

  for (const PropertyHandle& property : m_pendingUpdate.finishedTransitions()) {
    // This transition can also be cancelled and finished at the same time
    if (m_transitions.contains(property)) {
      Animation* animation = m_transitions.take(property).animation;
      // Transition must be downgraded
      if (animation->effect() &&
          animation->effect()->isKeyframeEffectReadOnly())
        toKeyframeEffectReadOnly(animation->effect())->downgradeToNormal();
    }
  }

  for (const auto& entry : m_pendingUpdate.newTransitions()) {
    const CSSAnimationUpdate::NewTransition& newTransition = entry.value;

    RunningTransition runningTransition;
    runningTransition.from = newTransition.from;
    runningTransition.to = newTransition.to;
    runningTransition.reversingAdjustedStartValue =
        newTransition.reversingAdjustedStartValue;
    runningTransition.reversingShorteningFactor =
        newTransition.reversingShorteningFactor;

    const PropertyHandle& property = newTransition.property;
    const InertEffect* inertAnimation = newTransition.effect.get();
    TransitionEventDelegate* eventDelegate =
        new TransitionEventDelegate(element, property);

    EffectModel* model = inertAnimation->model();

    if (retargetedCompositorTransitions.contains(property)) {
      const std::pair<Member<KeyframeEffectReadOnly>, double>& oldTransition =
          retargetedCompositorTransitions.at(property);
      KeyframeEffectReadOnly* oldAnimation = oldTransition.first;
      double oldStartTime = oldTransition.second;
      double inheritedTime =
          isNull(oldStartTime)
              ? 0
              : element->document().timeline().currentTimeInternal() -
                    oldStartTime;

      TransitionKeyframeEffectModel* oldEffect =
          toTransitionKeyframeEffectModel(inertAnimation->model());
      const KeyframeVector& frames = oldEffect->getFrames();

      TransitionKeyframeVector newFrames;
      newFrames.push_back(toTransitionKeyframe(frames[0]->clone().get()));
      newFrames.push_back(toTransitionKeyframe(frames[1]->clone().get()));
      newFrames.push_back(toTransitionKeyframe(frames[2]->clone().get()));

      InertEffect* inertAnimationForSampling = InertEffect::create(
          oldAnimation->model(), oldAnimation->specifiedTiming(), false,
          inheritedTime);
      Vector<RefPtr<Interpolation>> sample;
      inertAnimationForSampling->sample(sample);
      if (sample.size() == 1) {
        const TransitionInterpolation& interpolation =
            toTransitionInterpolation(*sample.at(0));
        newFrames[0]->setValue(interpolation.getInterpolatedValue());
        newFrames[0]->setCompositorValue(
            interpolation.getInterpolatedCompositorValue());
        newFrames[1]->setValue(interpolation.getInterpolatedValue());
        newFrames[1]->setCompositorValue(
            interpolation.getInterpolatedCompositorValue());
        model = TransitionKeyframeEffectModel::create(newFrames);
      }
    }

    KeyframeEffect* transition = KeyframeEffect::create(
        element, model, inertAnimation->specifiedTiming(),
        KeyframeEffectReadOnly::TransitionPriority, eventDelegate);
    Animation* animation = element->document().timeline().play(transition);
    if (property.isCSSCustomProperty()) {
      animation->setId(property.customPropertyName());
    } else {
      animation->setId(getPropertyName(property.cssProperty()));
    }
    // Set the current time as the start time for retargeted transitions
    if (retargetedCompositorTransitions.contains(property))
      animation->setStartTime(element->document().timeline().currentTime());
    animation->update(TimingUpdateOnDemand);
    runningTransition.animation = animation;
    m_transitions.set(property, runningTransition);
    DCHECK(isValidCSSPropertyID(property.cssProperty()));
    UseCounter::countAnimatedCSS(element->document(), property.cssProperty());

    // TODO(crbug.com/458925): Remove legacy histogram and counts
    DEFINE_STATIC_LOCAL(SparseHistogram, propertyHistogram,
                        ("WebCore.Animation.CSSProperties"));
    propertyHistogram.sample(
        UseCounter::mapCSSPropertyIdToCSSSampleIdForHistogram(
            property.cssProperty()));
  }
  clearPendingUpdate();
}

void CSSAnimations::calculateTransitionUpdateForProperty(
    TransitionUpdateState& state,
    const PropertyHandle& property,
    size_t transitionIndex) {
  state.listedProperties.insert(property);

  // FIXME: We should transition if an !important property changes even when an
  // animation is running, but this is a bit hard to do with the current
  // applyMatchedProperties system.
  if (state.update.activeInterpolationsForAnimations().contains(property) ||
      (state.animatingElement->elementAnimations() &&
       state.animatingElement->elementAnimations()
           ->cssAnimations()
           .m_previousActiveInterpolationsForAnimations.contains(property))) {
    return;
  }

  RefPtr<AnimatableValue> to = nullptr;
  const RunningTransition* interruptedTransition = nullptr;
  if (state.activeTransitions) {
    TransitionMap::const_iterator activeTransitionIter =
        state.activeTransitions->find(property);
    if (activeTransitionIter != state.activeTransitions->end()) {
      const RunningTransition* runningTransition = &activeTransitionIter->value;
      to = CSSAnimatableValueFactory::create(property, state.style);
      const AnimatableValue* activeTo = runningTransition->to.get();
      if (to->equals(activeTo))
        return;
      state.update.cancelTransition(property);
      DCHECK(!state.animatingElement->elementAnimations() ||
             !state.animatingElement->elementAnimations()
                  ->isAnimationStyleChange());

      if (to->equals(runningTransition->reversingAdjustedStartValue.get())) {
        interruptedTransition = runningTransition;
      }
    }
  }

  const PropertyRegistry* registry =
      state.animatingElement->document().propertyRegistry();

  if (property.isCSSCustomProperty()) {
    if (!registry || !registry->registration(property.customPropertyName()) ||
        CSSPropertyEquality::registeredCustomPropertiesEqual(
            property.customPropertyName(), state.oldStyle, state.style)) {
      return;
    }
  } else if (CSSPropertyEquality::propertiesEqual(
                 property.cssProperty(), state.oldStyle, state.style)) {
    return;
  }

  if (!to)
    to = CSSAnimatableValueFactory::create(property, state.style);
  RefPtr<AnimatableValue> from =
      CSSAnimatableValueFactory::create(property, state.oldStyle);

  CSSInterpolationTypesMap map(registry);
  InterpolationEnvironment oldEnvironment(map, state.oldStyle);
  InterpolationEnvironment newEnvironment(map, state.style);
  InterpolationValue start = nullptr;
  InterpolationValue end = nullptr;
  const InterpolationType* transitionType = nullptr;
  for (const auto& interpolationType : map.get(property)) {
    start = interpolationType->maybeConvertUnderlyingValue(oldEnvironment);
    if (!start) {
      continue;
    }
    end = interpolationType->maybeConvertUnderlyingValue(newEnvironment);
    if (!end) {
      continue;
    }
    // Merge will only succeed if the two values are considered interpolable.
    if (interpolationType->maybeMergeSingles(start.clone(), end.clone())) {
      transitionType = interpolationType.get();
      break;
    }
  }

  // No smooth interpolation exists between these values so don't start a
  // transition.
  if (!transitionType) {
    return;
  }

  // If we have multiple transitions on the same property, we will use the
  // last one since we iterate over them in order.

  Timing timing = state.transitionData.convertToTiming(transitionIndex);
  if (timing.startDelay + timing.iterationDuration <= 0) {
    // We may have started a transition in a prior CSSTransitionData update,
    // this CSSTransitionData update needs to override them.
    // TODO(alancutter): Just iterate over the CSSTransitionDatas in reverse and
    // skip any properties that have already been visited so we don't need to
    // "undo" work like this.
    state.update.unstartTransition(property);
    return;
  }

  AnimatableValue* reversingAdjustedStartValue = from.get();
  double reversingShorteningFactor = 1;
  if (interruptedTransition) {
    const double interruptedProgress =
        interruptedTransition->animation->effect()->progress();
    if (!std::isnan(interruptedProgress)) {
      reversingAdjustedStartValue = interruptedTransition->to.get();
      reversingShorteningFactor =
          clampTo((interruptedProgress *
                   interruptedTransition->reversingShorteningFactor) +
                      (1 - interruptedTransition->reversingShorteningFactor),
                  0.0, 1.0);
      timing.iterationDuration *= reversingShorteningFactor;
      if (timing.startDelay < 0) {
        timing.startDelay *= reversingShorteningFactor;
      }
    }
  }

  TransitionKeyframeVector keyframes;
  double startKeyframeOffset = 0;

  if (timing.startDelay > 0) {
    timing.iterationDuration += timing.startDelay;
    startKeyframeOffset = timing.startDelay / timing.iterationDuration;
    timing.startDelay = 0;
  }

  RefPtr<TransitionKeyframe> delayKeyframe =
      TransitionKeyframe::create(property);
  delayKeyframe->setValue(TypedInterpolationValue::create(
      *transitionType, start.interpolableValue->clone(),
      start.nonInterpolableValue));
  delayKeyframe->setOffset(0);
  keyframes.push_back(delayKeyframe);

  RefPtr<TransitionKeyframe> startKeyframe =
      TransitionKeyframe::create(property);
  startKeyframe->setValue(TypedInterpolationValue::create(
      *transitionType, start.interpolableValue->clone(),
      start.nonInterpolableValue));
  startKeyframe->setOffset(startKeyframeOffset);
  startKeyframe->setEasing(std::move(timing.timingFunction));
  timing.timingFunction = LinearTimingFunction::shared();
  keyframes.push_back(startKeyframe);

  RefPtr<TransitionKeyframe> endKeyframe = TransitionKeyframe::create(property);
  endKeyframe->setValue(TypedInterpolationValue::create(
      *transitionType, end.interpolableValue->clone(),
      end.nonInterpolableValue));
  endKeyframe->setOffset(1);
  keyframes.push_back(endKeyframe);

  if (CompositorAnimations::isCompositableProperty(property.cssProperty())) {
    delayKeyframe->setCompositorValue(from);
    startKeyframe->setCompositorValue(from);
    endKeyframe->setCompositorValue(to);
  }

  TransitionKeyframeEffectModel* model =
      TransitionKeyframeEffectModel::create(keyframes);
  state.update.startTransition(
      property, from.get(), to.get(), reversingAdjustedStartValue,
      reversingShorteningFactor, *InertEffect::create(model, timing, false, 0));
  DCHECK(
      !state.animatingElement->elementAnimations() ||
      !state.animatingElement->elementAnimations()->isAnimationStyleChange());
}

void CSSAnimations::calculateTransitionUpdateForCustomProperty(
    TransitionUpdateState& state,
    const CSSTransitionData::TransitionProperty& transitionProperty,
    size_t transitionIndex) {
  if (transitionProperty.propertyType !=
      CSSTransitionData::TransitionUnknownProperty) {
    return;
  }
  if (!CSSVariableParser::isValidVariableName(
          transitionProperty.propertyString)) {
    return;
  }
  calculateTransitionUpdateForProperty(
      state, PropertyHandle(transitionProperty.propertyString),
      transitionIndex);
}

void CSSAnimations::calculateTransitionUpdateForStandardProperty(
    TransitionUpdateState& state,
    const CSSTransitionData::TransitionProperty& transitionProperty,
    size_t transitionIndex) {
  if (transitionProperty.propertyType !=
      CSSTransitionData::TransitionKnownProperty) {
    return;
  }

  CSSPropertyID resolvedID =
      resolveCSSPropertyID(transitionProperty.unresolvedProperty);
  bool animateAll = resolvedID == CSSPropertyAll;
  const StylePropertyShorthand& propertyList =
      animateAll ? propertiesForTransitionAll()
                 : shorthandForProperty(resolvedID);
  // If not a shorthand we only execute one iteration of this loop, and
  // refer to the property directly.
  for (unsigned i = 0; !i || i < propertyList.length(); ++i) {
    CSSPropertyID longhandID =
        propertyList.length() ? propertyList.properties()[i] : resolvedID;
    PropertyHandle property = PropertyHandle(longhandID);
    DCHECK_GE(longhandID, firstCSSProperty);

    if (!animateAll &&
        !CSSPropertyMetadata::isInterpolableProperty(longhandID)) {
      continue;
    }

    calculateTransitionUpdateForProperty(state, property, transitionIndex);
  }
}

void CSSAnimations::calculateTransitionUpdate(CSSAnimationUpdate& update,
                                              PropertyPass propertyPass,
                                              const Element* animatingElement,
                                              const ComputedStyle& style) {
  if (!animatingElement)
    return;

  if (animatingElement->document().finishingOrIsPrinting())
    return;

  ElementAnimations* elementAnimations = animatingElement->elementAnimations();
  const TransitionMap* activeTransitions =
      elementAnimations ? &elementAnimations->cssAnimations().m_transitions
                        : nullptr;
  const CSSTransitionData* transitionData = style.transitions();

  const bool animationStyleRecalc =
      elementAnimations && elementAnimations->isAnimationStyleChange();

  HashSet<PropertyHandle> listedProperties;
  bool anyTransitionHadTransitionAll = false;
  const LayoutObject* layoutObject = animatingElement->layoutObject();
  if (!animationStyleRecalc && style.display() != EDisplay::kNone &&
      layoutObject && layoutObject->style() && transitionData) {
    TransitionUpdateState state = {
        update,         animatingElement,  *layoutObject->style(),
        style,          activeTransitions, listedProperties,
        *transitionData};

    for (size_t transitionIndex = 0;
         transitionIndex < transitionData->propertyList().size();
         ++transitionIndex) {
      const CSSTransitionData::TransitionProperty& transitionProperty =
          transitionData->propertyList()[transitionIndex];
      if (transitionProperty.unresolvedProperty == CSSPropertyAll) {
        anyTransitionHadTransitionAll = true;
      }
      if (propertyPass == PropertyPass::Custom) {
        calculateTransitionUpdateForCustomProperty(state, transitionProperty,
                                                   transitionIndex);
      } else {
        DCHECK_EQ(propertyPass, PropertyPass::Standard);
        calculateTransitionUpdateForStandardProperty(state, transitionProperty,
                                                     transitionIndex);
      }
    }
  }

  if (activeTransitions) {
    for (const auto& entry : *activeTransitions) {
      const PropertyHandle& property = entry.key;
      if (property.isCSSCustomProperty() !=
          (propertyPass == PropertyPass::Custom)) {
        continue;
      }
      if (!anyTransitionHadTransitionAll && !animationStyleRecalc &&
          !listedProperties.contains(property)) {
        update.cancelTransition(property);
      } else if (entry.value.animation->finishedInternal()) {
        update.finishTransition(property);
      }
    }
  }

  calculateTransitionActiveInterpolations(update, propertyPass,
                                          animatingElement);
}

void CSSAnimations::cancel() {
  for (const auto& runningAnimation : m_runningAnimations) {
    runningAnimation->animation->cancel();
    runningAnimation->animation->update(TimingUpdateOnDemand);
  }

  for (const auto& entry : m_transitions) {
    entry.value.animation->cancel();
    entry.value.animation->update(TimingUpdateOnDemand);
  }

  m_runningAnimations.clear();
  m_transitions.clear();
  clearPendingUpdate();
}

// TODO(alancutter): CSS properties and presentation attributes may have
// identical effects. By grouping them in the same set we introduce a bug where
// arbitrary hash iteration will determine the order the apply in and thus which
// one "wins". We should be more deliberate about the order of application in
// the case of effect collisions.
// Example: Both 'color' and 'svg-color' set the color on ComputedStyle but are
// considered distinct properties in the ActiveInterpolationsMap.
static bool isStylePropertyHandle(const PropertyHandle& propertyHandle) {
  return propertyHandle.isCSSProperty() ||
         propertyHandle.isPresentationAttribute();
}

void CSSAnimations::calculateAnimationActiveInterpolations(
    CSSAnimationUpdate& update,
    const Element* animatingElement) {
  ElementAnimations* elementAnimations =
      animatingElement ? animatingElement->elementAnimations() : nullptr;
  EffectStack* effectStack =
      elementAnimations ? &elementAnimations->effectStack() : nullptr;

  if (update.newAnimations().isEmpty() &&
      update.suppressedAnimations().isEmpty()) {
    ActiveInterpolationsMap activeInterpolationsForAnimations(
        EffectStack::activeInterpolations(
            effectStack, nullptr, nullptr,
            KeyframeEffectReadOnly::DefaultPriority, isStylePropertyHandle));
    update.adoptActiveInterpolationsForAnimations(
        activeInterpolationsForAnimations);
    return;
  }

  HeapVector<Member<const InertEffect>> newEffects;
  for (const auto& newAnimation : update.newAnimations())
    newEffects.push_back(newAnimation.effect);

  // Animations with updates use a temporary InertEffect for the current frame.
  for (const auto& updatedAnimation : update.animationsWithUpdates())
    newEffects.push_back(updatedAnimation.effect);

  ActiveInterpolationsMap activeInterpolationsForAnimations(
      EffectStack::activeInterpolations(
          effectStack, &newEffects, &update.suppressedAnimations(),
          KeyframeEffectReadOnly::DefaultPriority, isStylePropertyHandle));
  update.adoptActiveInterpolationsForAnimations(
      activeInterpolationsForAnimations);
}

static bool isCustomStylePropertyHandle(const PropertyHandle& property) {
  return property.isCSSCustomProperty();
}

static bool isStandardStylePropertyHandle(const PropertyHandle& property) {
  return isStylePropertyHandle(property) && !property.isCSSCustomProperty();
}

static EffectStack::PropertyHandleFilter stylePropertyFilter(
    CSSAnimations::PropertyPass propertyPass) {
  if (propertyPass == CSSAnimations::PropertyPass::Custom) {
    return isCustomStylePropertyHandle;
  }
  DCHECK_EQ(propertyPass, CSSAnimations::PropertyPass::Standard);
  return isStandardStylePropertyHandle;
}

void CSSAnimations::calculateTransitionActiveInterpolations(
    CSSAnimationUpdate& update,
    PropertyPass propertyPass,
    const Element* animatingElement) {
  ElementAnimations* elementAnimations =
      animatingElement ? animatingElement->elementAnimations() : nullptr;
  EffectStack* effectStack =
      elementAnimations ? &elementAnimations->effectStack() : nullptr;

  ActiveInterpolationsMap activeInterpolationsForTransitions;
  if (update.newTransitions().isEmpty() &&
      update.cancelledTransitions().isEmpty()) {
    activeInterpolationsForTransitions = EffectStack::activeInterpolations(
        effectStack, nullptr, nullptr,
        KeyframeEffectReadOnly::TransitionPriority,
        stylePropertyFilter(propertyPass));
  } else {
    HeapVector<Member<const InertEffect>> newTransitions;
    for (const auto& entry : update.newTransitions())
      newTransitions.push_back(entry.value.effect.get());

    HeapHashSet<Member<const Animation>> cancelledAnimations;
    if (!update.cancelledTransitions().isEmpty()) {
      DCHECK(elementAnimations);
      const TransitionMap& transitionMap =
          elementAnimations->cssAnimations().m_transitions;
      for (const PropertyHandle& property : update.cancelledTransitions()) {
        DCHECK(transitionMap.contains(property));
        cancelledAnimations.insert(transitionMap.at(property).animation.get());
      }
    }

    activeInterpolationsForTransitions = EffectStack::activeInterpolations(
        effectStack, &newTransitions, &cancelledAnimations,
        KeyframeEffectReadOnly::TransitionPriority,
        stylePropertyFilter(propertyPass));
  }

  // Properties being animated by animations don't get values from transitions
  // applied.
  if (!update.activeInterpolationsForAnimations().isEmpty() &&
      !activeInterpolationsForTransitions.isEmpty()) {
    for (const auto& entry : update.activeInterpolationsForAnimations())
      activeInterpolationsForTransitions.erase(entry.key);
  }

  if (propertyPass == PropertyPass::Custom) {
    update.adoptActiveInterpolationsForCustomTransitions(
        activeInterpolationsForTransitions);
  } else {
    DCHECK_EQ(propertyPass, PropertyPass::Standard);
    update.adoptActiveInterpolationsForStandardTransitions(
        activeInterpolationsForTransitions);
  }
}

EventTarget* CSSAnimations::AnimationEventDelegate::eventTarget() const {
  return EventPath::eventTargetRespectingTargetRules(*m_animationTarget);
}

void CSSAnimations::AnimationEventDelegate::maybeDispatch(
    Document::ListenerType listenerType,
    const AtomicString& eventName,
    double elapsedTime) {
  if (m_animationTarget->document().hasListenerType(listenerType)) {
    AnimationEvent* event =
        AnimationEvent::create(eventName, m_name, elapsedTime);
    event->setTarget(eventTarget());
    document().enqueueAnimationFrameEvent(event);
  }
}

bool CSSAnimations::AnimationEventDelegate::requiresIterationEvents(
    const AnimationEffectReadOnly& animationNode) {
  return document().hasListenerType(Document::ANIMATIONITERATION_LISTENER);
}

void CSSAnimations::AnimationEventDelegate::onEventCondition(
    const AnimationEffectReadOnly& animationNode) {
  const AnimationEffectReadOnly::Phase currentPhase = animationNode.getPhase();
  const double currentIteration = animationNode.currentIteration();

  if (m_previousPhase != currentPhase &&
      (currentPhase == AnimationEffectReadOnly::PhaseActive ||
       currentPhase == AnimationEffectReadOnly::PhaseAfter) &&
      (m_previousPhase == AnimationEffectReadOnly::PhaseNone ||
       m_previousPhase == AnimationEffectReadOnly::PhaseBefore)) {
    const double startDelay = animationNode.specifiedTiming().startDelay;
    const double elapsedTime = startDelay < 0 ? -startDelay : 0;
    maybeDispatch(Document::ANIMATIONSTART_LISTENER,
                  EventTypeNames::animationstart, elapsedTime);
  }

  if (currentPhase == AnimationEffectReadOnly::PhaseActive &&
      m_previousPhase == currentPhase &&
      m_previousIteration != currentIteration) {
    // We fire only a single event for all iterations thast terminate
    // between a single pair of samples. See http://crbug.com/275263. For
    // compatibility with the existing implementation, this event uses
    // the elapsedTime for the first iteration in question.
    DCHECK(!std::isnan(animationNode.specifiedTiming().iterationDuration));
    const double elapsedTime =
        animationNode.specifiedTiming().iterationDuration *
        (m_previousIteration + 1);
    maybeDispatch(Document::ANIMATIONITERATION_LISTENER,
                  EventTypeNames::animationiteration, elapsedTime);
  }

  if (currentPhase == AnimationEffectReadOnly::PhaseAfter &&
      m_previousPhase != AnimationEffectReadOnly::PhaseAfter)
    maybeDispatch(Document::ANIMATIONEND_LISTENER, EventTypeNames::animationend,
                  animationNode.activeDurationInternal());

  m_previousPhase = currentPhase;
  m_previousIteration = currentIteration;
}

DEFINE_TRACE(CSSAnimations::AnimationEventDelegate) {
  visitor->trace(m_animationTarget);
  AnimationEffectReadOnly::EventDelegate::trace(visitor);
}

EventTarget* CSSAnimations::TransitionEventDelegate::eventTarget() const {
  return EventPath::eventTargetRespectingTargetRules(*m_transitionTarget);
}

void CSSAnimations::TransitionEventDelegate::onEventCondition(
    const AnimationEffectReadOnly& animationNode) {
  const AnimationEffectReadOnly::Phase currentPhase = animationNode.getPhase();
  if (currentPhase == AnimationEffectReadOnly::PhaseAfter &&
      currentPhase != m_previousPhase &&
      document().hasListenerType(Document::TRANSITIONEND_LISTENER)) {
    String propertyName = m_property.isCSSCustomProperty()
                              ? m_property.customPropertyName()
                              : getPropertyNameString(m_property.cssProperty());
    const Timing& timing = animationNode.specifiedTiming();
    double elapsedTime = timing.iterationDuration;
    const AtomicString& eventType = EventTypeNames::transitionend;
    String pseudoElement =
        PseudoElement::pseudoElementNameForEvents(getPseudoId());
    TransitionEvent* event = TransitionEvent::create(
        eventType, propertyName, elapsedTime, pseudoElement);
    event->setTarget(eventTarget());
    document().enqueueAnimationFrameEvent(event);
  }

  m_previousPhase = currentPhase;
}

DEFINE_TRACE(CSSAnimations::TransitionEventDelegate) {
  visitor->trace(m_transitionTarget);
  AnimationEffectReadOnly::EventDelegate::trace(visitor);
}

const StylePropertyShorthand& CSSAnimations::propertiesForTransitionAll() {
  DEFINE_STATIC_LOCAL(Vector<CSSPropertyID>, properties, ());
  DEFINE_STATIC_LOCAL(StylePropertyShorthand, propertyShorthand, ());
  if (properties.isEmpty()) {
    for (int i = firstCSSProperty; i < lastCSSProperty; ++i) {
      CSSPropertyID id = convertToCSSPropertyID(i);
      // Avoid creating overlapping transitions with perspective-origin and
      // transition-origin.
      if (id == CSSPropertyWebkitPerspectiveOriginX ||
          id == CSSPropertyWebkitPerspectiveOriginY ||
          id == CSSPropertyWebkitTransformOriginX ||
          id == CSSPropertyWebkitTransformOriginY ||
          id == CSSPropertyWebkitTransformOriginZ)
        continue;
      if (CSSPropertyMetadata::isInterpolableProperty(id))
        properties.push_back(id);
    }
    propertyShorthand = StylePropertyShorthand(
        CSSPropertyInvalid, properties.begin(), properties.size());
  }
  return propertyShorthand;
}

// Properties that affect animations are not allowed to be affected by
// animations. http://w3c.github.io/web-animations/#not-animatable-section
bool CSSAnimations::isAnimationAffectingProperty(CSSPropertyID property) {
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
      return true;
    default:
      return false;
  }
}

bool CSSAnimations::isAffectedByKeyframesFromScope(const Element& element,
                                                   const TreeScope& treeScope) {
  // Animated elements are affected by @keyframes rules from the same scope
  // and from their shadow sub-trees if they are shadow hosts.
  if (element.treeScope() == treeScope)
    return true;
  if (!isShadowHost(element))
    return false;
  if (treeScope.rootNode() == treeScope.document())
    return false;
  return toShadowRoot(treeScope.rootNode()).host() == element;
}

bool CSSAnimations::isCustomPropertyHandle(const PropertyHandle& property) {
  return property.isCSSProperty() &&
         property.cssProperty() == CSSPropertyVariable;
}

bool CSSAnimations::isAnimatingCustomProperties(
    const ElementAnimations* elementAnimations) {
  return elementAnimations &&
         elementAnimations->effectStack().affectsProperties(
             isCustomPropertyHandle);
}

DEFINE_TRACE(CSSAnimations) {
  visitor->trace(m_transitions);
  visitor->trace(m_pendingUpdate);
  visitor->trace(m_runningAnimations);
}

}  // namespace blink
