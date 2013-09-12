/*
 * Copyright (C) 2007, 2008, 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/page/animation/AnimationController.h"

#include "core/dom/EventNames.h"
#include "core/dom/PseudoElement.h"
#include "core/dom/TransitionEvent.h"
#include "core/dom/WebKitAnimationEvent.h"
#include "core/page/Frame.h"
#include "core/page/FrameView.h"
#include "core/page/Page.h"
#include "core/page/animation/AnimationBase.h"
#include "core/page/animation/AnimationControllerPrivate.h"
#include "core/page/animation/CSSPropertyAnimation.h"
#include "core/page/animation/CompositeAnimation.h"
#include "core/rendering/RenderView.h"
#include "wtf/CurrentTime.h"

namespace WebCore {

static const double cBeginAnimationUpdateTimeNotSet = -1;

AnimationControllerPrivate::AnimationControllerPrivate(Frame* frame)
    : m_animationTimer(this, &AnimationControllerPrivate::animationTimerFired)
    , m_updateStyleIfNeededDispatcher(this, &AnimationControllerPrivate::updateStyleIfNeededDispatcherFired)
    , m_frame(frame)
    , m_beginAnimationUpdateTime(cBeginAnimationUpdateTimeNotSet)
    , m_animationsWaitingForStyle()
    , m_animationsWaitingForStartTimeResponse()
    , m_waitingForAsyncStartNotification(false)
{
}

AnimationControllerPrivate::~AnimationControllerPrivate()
{
}

PassRefPtr<CompositeAnimation> AnimationControllerPrivate::accessCompositeAnimation(RenderObject* renderer)
{
    RefPtr<CompositeAnimation> animation = m_compositeAnimations.get(renderer);
    if (!animation) {
        animation = CompositeAnimation::create(this);
        m_compositeAnimations.set(renderer, animation);
    }
    return animation;
}

bool AnimationControllerPrivate::clear(RenderObject* renderer)
{
    // Return false if we didn't do anything OR we are suspended (so we don't try to
    // do a setNeedsStyleRecalc() when suspended).
    PassRefPtr<CompositeAnimation> animation = m_compositeAnimations.take(renderer);
    if (!animation)
        return false;
    animation->clearRenderer();
    return animation->suspended();
}

void AnimationControllerPrivate::updateAnimations(double& timeToNextService, double& timeToNextEvent, SetNeedsStyleRecalc callSetNeedsStyleRecalc/* = DoNotCallSetNeedsStyleRecalc*/)
{
    double minTimeToNextService = -1;
    double minTimeToNextEvent = -1;
    bool updateStyleNeeded = false;

    RenderObjectAnimationMap::const_iterator animationsEnd = m_compositeAnimations.end();
    for (RenderObjectAnimationMap::const_iterator it = m_compositeAnimations.begin(); it != animationsEnd; ++it) {
        CompositeAnimation* compAnim = it->value.get();
        if (!compAnim->suspended() && compAnim->hasAnimations()) {
            double t = compAnim->timeToNextService();
            if (t != -1 && (t < minTimeToNextService || minTimeToNextService == -1))
                minTimeToNextService = t;
            double nextEvent = compAnim->timeToNextEvent();
            if (nextEvent != -1 && (nextEvent < minTimeToNextEvent || minTimeToNextEvent == -1))
                minTimeToNextEvent = nextEvent;
            if (callSetNeedsStyleRecalc == CallSetNeedsStyleRecalc) {
                if (!t) {
                    Node* node = it->key->node();
                    node->setNeedsStyleRecalc(LocalStyleChange, StyleChangeFromRenderer);
                    updateStyleNeeded = true;
                }
            } else if (!minTimeToNextService && !minTimeToNextEvent) {
                // Found the minimum values and do not need to mark for style recalc.
                break;
            }
        }
    }

    if (updateStyleNeeded)
        m_frame->document()->updateStyleIfNeeded();

    timeToNextService = minTimeToNextService;
    timeToNextEvent = minTimeToNextEvent;
}

void AnimationControllerPrivate::scheduleServiceForRenderer(RenderObject* renderer)
{
    double timeToNextService = -1;
    double timeToNextEvent = -1;

    RefPtr<CompositeAnimation> compAnim = m_compositeAnimations.get(renderer);
    if (!compAnim->suspended() && compAnim->hasAnimations()) {
        timeToNextService = compAnim->timeToNextService();
        timeToNextEvent = compAnim->timeToNextEvent();
    }

    if (timeToNextService >= 0)
        scheduleService(timeToNextService, timeToNextEvent);
}

void AnimationControllerPrivate::scheduleService()
{
    double timeToNextService = -1;
    double timeToNextEvent = -1;
    updateAnimations(timeToNextService, timeToNextEvent, DoNotCallSetNeedsStyleRecalc);
    scheduleService(timeToNextService, timeToNextEvent);
}

void AnimationControllerPrivate::scheduleService(double timeToNextService, double timeToNextEvent)
{
    if (!m_frame->page())
        return;

    bool visible = m_frame->page()->visibilityState() == WebCore::PageVisibilityStateVisible;

    // This std::max to 1 second limits how often we service animations on background tabs.
    // Without this, plus and gmail were recalculating style as every 200ms or even more
    // often, burning CPU needlessly for background tabs.
    // FIXME: Do we want to fire events at all on background tabs?
    if (!visible)
        timeToNextService = std::max(timeToNextEvent, 1.0);

    if (visible && !timeToNextService) {
        m_frame->document()->view()->scheduleAnimation();
        if (m_animationTimer.isActive())
            m_animationTimer.stop();
        return;
    }

    if (timeToNextService < 0) {
        if (m_animationTimer.isActive())
            m_animationTimer.stop();
        return;
    }

    if (m_animationTimer.isActive() && m_animationTimer.nextFireInterval() <= timeToNextService)
        return;

    m_animationTimer.startOneShot(timeToNextService);
}

void AnimationControllerPrivate::updateStyleIfNeededDispatcherFired(Timer<AnimationControllerPrivate>*)
{
    fireEventsAndUpdateStyle();
}

void AnimationControllerPrivate::fireEventsAndUpdateStyle()
{
    // Protect the frame from getting destroyed in the event handler
    RefPtr<Frame> protector = m_frame;

    bool updateStyle = !m_eventsToDispatch.isEmpty() || !m_nodeChangesToDispatch.isEmpty();

    // fire all the events
    Vector<EventToDispatch> eventsToDispatch = m_eventsToDispatch;
    m_eventsToDispatch.clear();
    Vector<EventToDispatch>::const_iterator eventsToDispatchEnd = eventsToDispatch.end();
    for (Vector<EventToDispatch>::const_iterator it = eventsToDispatch.begin(); it != eventsToDispatchEnd; ++it) {
        Element* element = it->element.get();
        if (it->eventType == eventNames().transitionendEvent)
            element->dispatchEvent(TransitionEvent::create(it->eventType, it->name, it->elapsedTime, PseudoElement::pseudoElementNameForEvents(element->pseudoId())));
        else
            element->dispatchEvent(WebKitAnimationEvent::create(it->eventType, it->name, it->elapsedTime));
    }

    // call setChanged on all the elements
    Vector<RefPtr<Node> >::const_iterator nodeChangesToDispatchEnd = m_nodeChangesToDispatch.end();
    for (Vector<RefPtr<Node> >::const_iterator it = m_nodeChangesToDispatch.begin(); it != nodeChangesToDispatchEnd; ++it)
        (*it)->setNeedsStyleRecalc(LocalStyleChange, StyleChangeFromRenderer);

    m_nodeChangesToDispatch.clear();

    if (updateStyle && m_frame)
        m_frame->document()->updateStyleIfNeeded();
}

void AnimationControllerPrivate::startUpdateStyleIfNeededDispatcher()
{
    if (!m_updateStyleIfNeededDispatcher.isActive())
        m_updateStyleIfNeededDispatcher.startOneShot(0);
}

void AnimationControllerPrivate::addEventToDispatch(PassRefPtr<Element> element, const AtomicString& eventType, const String& name, double elapsedTime)
{
    m_eventsToDispatch.grow(m_eventsToDispatch.size()+1);
    EventToDispatch& event = m_eventsToDispatch[m_eventsToDispatch.size()-1];
    event.element = element;
    event.eventType = eventType;
    event.name = name;
    event.elapsedTime = elapsedTime;

    startUpdateStyleIfNeededDispatcher();
}

void AnimationControllerPrivate::addNodeChangeToDispatch(PassRefPtr<Node> node)
{
    if (!node)
        return;

    m_nodeChangesToDispatch.append(node);
    startUpdateStyleIfNeededDispatcher();
}

void AnimationControllerPrivate::serviceAnimations()
{
    double timeToNextService = -1;
    double timeToNextEvent = -1;
    updateAnimations(timeToNextService, timeToNextEvent, CallSetNeedsStyleRecalc);
    scheduleService(timeToNextService, timeToNextEvent);

    // Fire events right away, to avoid a flash of unanimated style after an animation completes, and before
    // the 'end' event fires.
    fireEventsAndUpdateStyle();
}

void AnimationControllerPrivate::animationTimerFired(Timer<AnimationControllerPrivate>*)
{
    // Make sure animationUpdateTime is updated, so that it is current even if no
    // styleChange has happened (e.g. accelerated animations)
    setBeginAnimationUpdateTime(cBeginAnimationUpdateTimeNotSet);
    serviceAnimations();
}

bool AnimationControllerPrivate::isRunningAnimationOnRenderer(RenderObject* renderer, CSSPropertyID property, bool isRunningNow) const
{
    RefPtr<CompositeAnimation> animation = m_compositeAnimations.get(renderer);
    if (!animation)
        return false;

    return animation->isAnimatingProperty(property, false, isRunningNow);
}

bool AnimationControllerPrivate::isRunningAcceleratableAnimationOnRenderer(RenderObject *renderer) const
{
    RefPtr<CompositeAnimation> animation = m_compositeAnimations.get(renderer);
    if (!animation)
        return false;

    bool acceleratedOnly = false;
    bool isRunningNow = true;
    return animation->isAnimatingProperty(CSSPropertyOpacity, acceleratedOnly, isRunningNow)
        || animation->isAnimatingProperty(CSSPropertyWebkitTransform, acceleratedOnly, isRunningNow)
        || animation->isAnimatingProperty(CSSPropertyWebkitFilter, acceleratedOnly, isRunningNow);
}

bool AnimationControllerPrivate::isRunningAcceleratedAnimationOnRenderer(RenderObject* renderer, CSSPropertyID property, bool isRunningNow) const
{
    RefPtr<CompositeAnimation> animation = m_compositeAnimations.get(renderer);
    if (!animation)
        return false;

    return animation->isAnimatingProperty(property, true, isRunningNow);
}

void AnimationControllerPrivate::suspendAnimations()
{
    suspendAnimationsForDocument(m_frame->document());

    // Traverse subframes
    for (Frame* child = m_frame->tree()->firstChild(); child; child = child->tree()->nextSibling())
        child->animation()->suspendAnimations();
}

void AnimationControllerPrivate::resumeAnimations()
{
    resumeAnimationsForDocument(m_frame->document());

    // Traverse subframes
    for (Frame* child = m_frame->tree()->firstChild(); child; child = child->tree()->nextSibling())
        child->animation()->resumeAnimations();
}

void AnimationControllerPrivate::suspendAnimationsForDocument(Document* document)
{
    setBeginAnimationUpdateTime(cBeginAnimationUpdateTimeNotSet);

    RenderObjectAnimationMap::const_iterator animationsEnd = m_compositeAnimations.end();
    for (RenderObjectAnimationMap::const_iterator it = m_compositeAnimations.begin(); it != animationsEnd; ++it) {
        RenderObject* renderer = it->key;
        if (&renderer->document() == document) {
            CompositeAnimation* compAnim = it->value.get();
            compAnim->suspendAnimations();
        }
    }

    scheduleService();
}

void AnimationControllerPrivate::resumeAnimationsForDocument(Document* document)
{
    setBeginAnimationUpdateTime(cBeginAnimationUpdateTimeNotSet);

    RenderObjectAnimationMap::const_iterator animationsEnd = m_compositeAnimations.end();
    for (RenderObjectAnimationMap::const_iterator it = m_compositeAnimations.begin(); it != animationsEnd; ++it) {
        RenderObject* renderer = it->key;
        if (&renderer->document() == document) {
            CompositeAnimation* compAnim = it->value.get();
            compAnim->resumeAnimations();
        }
    }

    scheduleService();
}

void AnimationControllerPrivate::pauseAnimationsForTesting(double t)
{
    RenderObjectAnimationMap::const_iterator animationsEnd = m_compositeAnimations.end();
    for (RenderObjectAnimationMap::const_iterator it = m_compositeAnimations.begin(); it != animationsEnd; ++it) {
        it->value->pauseAnimationsForTesting(t);
        it->key->node()->setNeedsStyleRecalc(LocalStyleChange, StyleChangeFromRenderer);
    }
}

double AnimationControllerPrivate::beginAnimationUpdateTime()
{
    if (m_beginAnimationUpdateTime == cBeginAnimationUpdateTimeNotSet)
        m_beginAnimationUpdateTime = currentTime();
    return m_beginAnimationUpdateTime;
}

void AnimationControllerPrivate::endAnimationUpdate()
{
    styleAvailable();
    if (!m_waitingForAsyncStartNotification)
        startTimeResponse(beginAnimationUpdateTime());
}

void AnimationControllerPrivate::receivedStartTimeResponse(double time)
{
    m_waitingForAsyncStartNotification = false;
    startTimeResponse(time);
}

PassRefPtr<RenderStyle> AnimationControllerPrivate::getAnimatedStyleForRenderer(RenderObject* renderer)
{
    if (!renderer)
        return 0;

    RefPtr<CompositeAnimation> rendererAnimations = m_compositeAnimations.get(renderer);
    if (!rendererAnimations)
        return renderer->style();

    RefPtr<RenderStyle> animatingStyle = rendererAnimations->getAnimatedStyle();
    if (!animatingStyle)
        animatingStyle = renderer->style();

    return animatingStyle.release();
}

unsigned AnimationControllerPrivate::numberOfActiveAnimations(Document* document) const
{
    unsigned count = 0;

    RenderObjectAnimationMap::const_iterator animationsEnd = m_compositeAnimations.end();
    for (RenderObjectAnimationMap::const_iterator it = m_compositeAnimations.begin(); it != animationsEnd; ++it) {
        RenderObject* renderer = it->key;
        CompositeAnimation* compAnim = it->value.get();
        if (&renderer->document() == document)
            count += compAnim->numberOfActiveAnimations();
    }

    return count;
}

void AnimationControllerPrivate::addToAnimationsWaitingForStyle(AnimationBase* animation)
{
    // Make sure this animation is not in the start time waiters
    m_animationsWaitingForStartTimeResponse.remove(animation);

    m_animationsWaitingForStyle.add(animation);
}

void AnimationControllerPrivate::removeFromAnimationsWaitingForStyle(AnimationBase* animationToRemove)
{
    m_animationsWaitingForStyle.remove(animationToRemove);
}

void AnimationControllerPrivate::styleAvailable()
{
    // Go through list of waiters and send them on their way
    WaitingAnimationsSet::const_iterator it = m_animationsWaitingForStyle.begin();
    WaitingAnimationsSet::const_iterator end = m_animationsWaitingForStyle.end();
    for (; it != end; ++it)
        (*it)->styleAvailable();

    m_animationsWaitingForStyle.clear();
}

void AnimationControllerPrivate::addToAnimationsWaitingForStartTimeResponse(AnimationBase* animation, bool willGetResponse)
{
    // If willGetResponse is true, it means this animation is actually waiting for a response
    // (which will come in as a call to notifyAnimationStarted()).
    // In that case we don't need to add it to this list. We just set a waitingForAResponse flag
    // which says we are waiting for the response. If willGetResponse is false, this animation
    // is not waiting for a response for itself, but rather for a notifyXXXStarted() call for
    // another animation to which it will sync.
    //
    // When endAnimationUpdate() is called we check to see if the waitingForAResponse flag is
    // true. If so, we just return and will do our work when the first notifyXXXStarted() call
    // comes in. If it is false, we will not be getting a notifyXXXStarted() call, so we will
    // do our work right away. In both cases we call the onAnimationStartResponse() method
    // on each animation. In the first case we send in the time we got from notifyXXXStarted().
    // In the second case, we just pass in the beginAnimationUpdateTime().
    //
    // This will synchronize all software and accelerated animations started in the same
    // updateStyleIfNeeded cycle.
    //

    if (willGetResponse)
        m_waitingForAsyncStartNotification = true;

    m_animationsWaitingForStartTimeResponse.add(animation);
}

void AnimationControllerPrivate::removeFromAnimationsWaitingForStartTimeResponse(AnimationBase* animationToRemove)
{
    m_animationsWaitingForStartTimeResponse.remove(animationToRemove);

    if (m_animationsWaitingForStartTimeResponse.isEmpty())
        m_waitingForAsyncStartNotification = false;
}

void AnimationControllerPrivate::startTimeResponse(double time)
{
    // Go through list of waiters and send them on their way

    WaitingAnimationsSet::const_iterator it = m_animationsWaitingForStartTimeResponse.begin();
    WaitingAnimationsSet::const_iterator end = m_animationsWaitingForStartTimeResponse.end();
    for (; it != end; ++it)
        (*it)->onAnimationStartResponse(time);

    m_animationsWaitingForStartTimeResponse.clear();
    m_waitingForAsyncStartNotification = false;
}

void AnimationControllerPrivate::animationWillBeRemoved(AnimationBase* animation)
{
    removeFromAnimationsWaitingForStyle(animation);
    removeFromAnimationsWaitingForStartTimeResponse(animation);
}

AnimationController::AnimationController(Frame* frame)
    : m_data(adoptPtr(new AnimationControllerPrivate(frame)))
    , m_beginAnimationUpdateCount(0)
{
}

AnimationController::~AnimationController()
{
}

void AnimationController::cancelAnimations(RenderObject* renderer)
{
    if (!m_data->hasAnimations())
        return;

    if (m_data->clear(renderer)) {
        if (Node* node = renderer->node())
            node->setNeedsStyleRecalc(LocalStyleChange, StyleChangeFromRenderer);
    }
}

PassRefPtr<RenderStyle> AnimationController::updateAnimations(RenderObject* renderer, RenderStyle* newStyle)
{
    RenderStyle* oldStyle = renderer->style();

    if ((!oldStyle || (!oldStyle->animations() && !oldStyle->transitions())) && (!newStyle->animations() && !newStyle->transitions()))
        return newStyle;

    // Don't run transitions when printing.
    if (renderer->view()->document().printing())
        return newStyle;

    // Fetch our current set of implicit animations from a hashtable.  We then compare them
    // against the animations in the style and make sure we're in sync.  If destination values
    // have changed, we reset the animation.  We then do a blend to get new values and we return
    // a new style.

    // We don't support anonymous pseudo elements like :first-line or :first-letter.
    ASSERT(renderer->node());

    RefPtr<CompositeAnimation> rendererAnimations = m_data->accessCompositeAnimation(renderer);
    RefPtr<RenderStyle> blendedStyle = rendererAnimations->animate(renderer, oldStyle, newStyle);

    if (renderer->parent() || newStyle->animations() || (oldStyle && oldStyle->animations())) {
        m_data->scheduleServiceForRenderer(renderer);
    }

    if (blendedStyle != newStyle) {
        // If the animations/transitions change opacity or transform, we need to update
        // the style to impose the stacking rules. Note that this is also
        // done in StyleResolver::adjustRenderStyle().
        if (blendedStyle->hasAutoZIndex() && (blendedStyle->opacity() < 1.0f || blendedStyle->hasTransform()))
            blendedStyle->setZIndex(0);
    }
    return blendedStyle.release();
}

PassRefPtr<RenderStyle> AnimationController::getAnimatedStyleForRenderer(RenderObject* renderer)
{
    return m_data->getAnimatedStyleForRenderer(renderer);
}

void AnimationController::notifyAnimationStarted(RenderObject*, double startTime)
{
    m_data->receivedStartTimeResponse(startTime);
}

void AnimationController::pauseAnimationsForTesting(double t)
{
    m_data->pauseAnimationsForTesting(t);
}

unsigned AnimationController::numberOfActiveAnimations(Document* document) const
{
    return m_data->numberOfActiveAnimations(document);
}

bool AnimationController::isRunningAnimationOnRenderer(RenderObject* renderer, CSSPropertyID property, bool isRunningNow) const
{
    return m_data->isRunningAnimationOnRenderer(renderer, property, isRunningNow);
}

bool AnimationController::isRunningAcceleratableAnimationOnRenderer(RenderObject* renderer) const
{
    return m_data->isRunningAcceleratableAnimationOnRenderer(renderer);
}

bool AnimationController::isRunningAcceleratedAnimationOnRenderer(RenderObject* renderer, CSSPropertyID property, bool isRunningNow) const
{
    return m_data->isRunningAcceleratedAnimationOnRenderer(renderer, property, isRunningNow);
}

void AnimationController::suspendAnimations()
{
    m_data->suspendAnimations();
}

void AnimationController::resumeAnimations()
{
    m_data->resumeAnimations();
}

void AnimationController::serviceAnimations()
{
    m_data->serviceAnimations();
}

void AnimationController::suspendAnimationsForDocument(Document* document)
{
    m_data->suspendAnimationsForDocument(document);
}

void AnimationController::resumeAnimationsForDocument(Document* document)
{
    m_data->resumeAnimationsForDocument(document);
}

void AnimationController::beginAnimationUpdate()
{
    if (!m_beginAnimationUpdateCount)
        m_data->setBeginAnimationUpdateTime(cBeginAnimationUpdateTimeNotSet);
    ++m_beginAnimationUpdateCount;
}

void AnimationController::endAnimationUpdate()
{
    ASSERT(m_beginAnimationUpdateCount > 0);
    --m_beginAnimationUpdateCount;
    if (!m_beginAnimationUpdateCount)
        m_data->endAnimationUpdate();
}

bool AnimationController::supportsAcceleratedAnimationOfProperty(CSSPropertyID property)
{
    return CSSPropertyAnimation::animationOfPropertyIsAccelerated(property);
}

} // namespace WebCore
