// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/AutoplayExperimentHelper.h"

#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLMediaElement.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutVideo.h"
#include "core/layout/LayoutView.h"
#include "core/page/Page.h"
#include "platform/Logging.h"
#include "platform/UserGestureIndicator.h"
#include "platform/geometry/IntRect.h"

namespace blink {

using namespace HTMLNames;

// Seconds to wait after a video has stopped moving before playing it.
static const double kViewportTimerPollDelay = 0.5;

AutoplayExperimentHelper::AutoplayExperimentHelper(HTMLMediaElement& element)
    : m_element(&element)
    , m_mode(Mode::ExperimentOff)
    , m_playPending(false)
    , m_registeredWithLayoutObject(false)
    , m_wasInViewport(false)
    , m_lastLocationUpdateTime(-std::numeric_limits<double>::infinity())
    , m_viewportTimer(this, &AutoplayExperimentHelper::viewportTimerFired)
{
    if (document().settings()) {
        m_mode = fromString(document().settings()->autoplayExperimentMode());

        if (m_mode != Mode::ExperimentOff) {
            WTF_LOG(Media, "HTMLMediaElement: autoplay experiment set to %d",
                m_mode);
        }
    }
}

AutoplayExperimentHelper::~AutoplayExperimentHelper()
{
    unregisterForPositionUpdatesIfNeeded();
}

void AutoplayExperimentHelper::becameReadyToPlay()
{
    // Assuming that we're eligible to override the user gesture requirement,
    // either play if we meet the visibility checks, or install a listener
    // to wait for them to pass.
    if (isEligible()) {
        if (meetsVisibilityRequirements())
            prepareToPlay(GesturelessPlaybackStartedByAutoplayFlagImmediately);
        else
            registerForPositionUpdatesIfNeeded();
    }
}

void AutoplayExperimentHelper::playMethodCalled()
{
    // Set the pending state, even if the play isn't going to be pending.
    // Eligibility can change if, for example, the mute status changes.
    // Having this set is okay.
    m_playPending = true;

    if (!UserGestureIndicator::processingUserGesture()) {

        if (isEligible()) {
            // Remember that userGestureRequiredForPlay is required for
            // us to be eligible for the experiment.
            // If we are able to override the gesture requirement now, then
            // do so.  Otherwise, install an event listener if we need one.
            if (meetsVisibilityRequirements()) {
                // Override the gesture and play.
                prepareToPlay(GesturelessPlaybackStartedByPlayMethodImmediately);
            } else {
                // Wait for viewport visibility.
                registerForPositionUpdatesIfNeeded();
            }
        }

    } else if (element().isUserGestureRequiredForPlay()) {
        unregisterForPositionUpdatesIfNeeded();
    }
}

void AutoplayExperimentHelper::pauseMethodCalled()
{
    // Don't try to autoplay, if we would have.
    m_playPending = false;
    unregisterForPositionUpdatesIfNeeded();
}

void AutoplayExperimentHelper::mutedChanged()
{
    // If we are no longer eligible for the autoplay experiment, then also
    // quit listening for events.  If we are eligible, and if we should be
    // playing, then start playing.  In other words, start playing if
    // we just needed 'mute' to autoplay.
    if (!isEligible()) {
        unregisterForPositionUpdatesIfNeeded();
    } else {
        // Try to play.  If we can't, then install a listener.
        if (!maybeStartPlaying())
            registerForPositionUpdatesIfNeeded();
    }
}

void AutoplayExperimentHelper::registerForPositionUpdatesIfNeeded()
{
    // If we don't require that the player is in the viewport, then we don't
    // need the listener.
    if (!enabled(IfViewport)) {
        if (!enabled(IfPageVisible))
            return;
    }

    if (LayoutObject* layoutObject = element().layoutObject()) {
        LayoutMedia* layoutMedia = toLayoutMedia(layoutObject);
        layoutMedia->setRequestPositionUpdates(true);
    }

    // Set this unconditionally, in case we have no layout object yet.
    m_registeredWithLayoutObject = true;
}

void AutoplayExperimentHelper::unregisterForPositionUpdatesIfNeeded()
{
    if (m_registeredWithLayoutObject) {
        if (LayoutObject* obj = element().layoutObject()) {
            LayoutMedia* layoutMedia = toLayoutMedia(obj);
            layoutMedia->setRequestPositionUpdates(false);
        }
    }

    // Clear this unconditionally so that we don't re-register if we didn't
    // have a LayoutObject now, but get one later.
    m_registeredWithLayoutObject = false;
}

void AutoplayExperimentHelper::positionChanged(const IntRect& visibleRect)
{
    // Something, maybe position, has changed.  If applicable, start a
    // timer to look for the end of a scroll operation.
    // Don't do much work here.
    // Also note that we are called quite often, including when the
    // page becomes visible.  That's why we don't bother to register
    // for page visibility changes explicitly.

    m_lastVisibleRect = visibleRect;

    if (!element().layoutObject())
        return;

    IntRect currentLocation = element().layoutObject()->absoluteBoundingBoxRect();
    bool inViewport = meetsVisibilityRequirements();

    if (m_lastLocation != currentLocation) {
        m_lastLocationUpdateTime = monotonicallyIncreasingTime();
        m_lastLocation = currentLocation;
    }

    if (inViewport && !m_wasInViewport) {
        // Only reset the timer when we transition from not visible to
        // visible, because resetting the timer isn't cheap.
        m_viewportTimer.startOneShot(kViewportTimerPollDelay, BLINK_FROM_HERE);
    }
    m_wasInViewport = inViewport;
}

void AutoplayExperimentHelper::updatePositionNotificationRegistration()
{
    if (m_registeredWithLayoutObject) {
        LayoutMedia* layoutMedia = toLayoutMedia(element().layoutObject());
        layoutMedia->setRequestPositionUpdates(true);
    }
}

void AutoplayExperimentHelper::triggerAutoplayViewportCheckForTesting()
{
    FrameView* view = document().view();
    if (view)
        positionChanged(view->rootFrameToContents(view->computeVisibleArea()));

    // Make sure that the last update appears to be sufficiently far in the
    // past to appear that scrolling has stopped by now in viewportTimerFired.
    m_lastLocationUpdateTime = monotonicallyIncreasingTime() - kViewportTimerPollDelay - 1;
    viewportTimerFired(nullptr);
}

void AutoplayExperimentHelper::viewportTimerFired(Timer<AutoplayExperimentHelper>*)
{
    double now = monotonicallyIncreasingTime();
    double delta = now - m_lastLocationUpdateTime;
    if (delta < kViewportTimerPollDelay) {
        // If we are not visible, then skip the timer.  It will be started
        // again if we become visible again.
        if (m_wasInViewport)
            m_viewportTimer.startOneShot(kViewportTimerPollDelay - delta, BLINK_FROM_HERE);

        return;
    }

    // Sufficient time has passed since the last scroll that we'll
    // treat it as the end of scroll.  Autoplay if we should.
    maybeStartPlaying();
}

bool AutoplayExperimentHelper::meetsVisibilityRequirements() const
{
    if (enabled(IfPageVisible)
        && element().document().pageVisibilityState() != PageVisibilityStateVisible)
        return false;

    if (!enabled(IfViewport))
        return true;

    if (m_lastVisibleRect.isEmpty())
        return false;

    LayoutObject* layoutObject = element().layoutObject();
    if (!layoutObject)
        return false;

    IntRect currentLocation = layoutObject->absoluteBoundingBoxRect();

    // If element completely fills the screen, then truncate it to exactly
    // match the screen.  Any element that is wider just has to cover.
    if (currentLocation.x() <= m_lastVisibleRect.x()
        && currentLocation.x() + currentLocation.width() >= m_lastVisibleRect.x() + m_lastVisibleRect.width()) {
        currentLocation.setX(m_lastVisibleRect.x());
        currentLocation.setWidth(m_lastVisibleRect.width());
    }

    if (currentLocation.y() <= m_lastVisibleRect.y()
        && currentLocation.y() + currentLocation.height() >= m_lastVisibleRect.y() + m_lastVisibleRect.height()) {
        currentLocation.setY(m_lastVisibleRect.y());
        currentLocation.setHeight(m_lastVisibleRect.height());
    }

    return m_lastVisibleRect.contains(currentLocation);
}

bool AutoplayExperimentHelper::maybeStartPlaying()
{
    // See if we're allowed to autoplay now.
    if (!isEligible() || !meetsVisibilityRequirements()) {
        return false;
    }

    // Start playing!
    prepareToPlay(element().shouldAutoplay()
        ? GesturelessPlaybackStartedByAutoplayFlagAfterScroll
        : GesturelessPlaybackStartedByPlayMethodAfterScroll);
    element().playInternal();

    return true;
}

bool AutoplayExperimentHelper::isEligible() const
{
    if (m_mode == Mode::ExperimentOff)
        return false;

    // If no user gesture is required, then the experiment doesn't apply.
    // This is what prevents us from starting playback more than once.
    // Since this flag is never set to true once it's cleared, it will block
    // the autoplay experiment forever.
    if (!element().isUserGestureRequiredForPlay())
        return false;

    // Make sure that this is an element of the right type.
    if (!enabled(ForVideo) && isHTMLVideoElement(element()))
        return false;

    if (!enabled(ForAudio) && isHTMLAudioElement(element()))
        return false;

    // If nobody has requested playback, either by the autoplay attribute or
    // a play() call, then do nothing.

    if (!m_playPending && !element().shouldAutoplay())
        return false;

    // Note that the viewport test always returns false on desktop, which is
    // why video-autoplay-experiment.html doesn't check -ifmobile .
    if (enabled(IfMobile)
        && !document().viewportDescription().isLegacyViewportType())
        return false;

    // If we require muted media and this is muted, then it is eligible.
    if (enabled(IfMuted))
        return element().muted();

    // Element is eligible for gesture override, maybe muted.
    return true;
}

void AutoplayExperimentHelper::muteIfNeeded()
{
    if (enabled(PlayMuted)) {
        ASSERT(!isEligible());
        // If we are actually changing the muted state, then this will call
        // mutedChanged().  If isEligible(), then mutedChanged() will try
        // to start playback, which we should not do here.
        element().setMuted(true);
    }
}

void AutoplayExperimentHelper::prepareToPlay(AutoplayMetrics metric)
{
    element().recordAutoplayMetric(metric);

    // This also causes !isEligible, so that we don't allow autoplay more than
    // once.  Be sure to do this before muteIfNeeded().
    element().removeUserGestureRequirement();

    unregisterForPositionUpdatesIfNeeded();
    muteIfNeeded();

    // Record that this autoplayed without a user gesture.  This is normally
    // set when we discover an autoplay attribute, but we include all cases
    // where playback started without a user gesture, e.g., play().
    element().setInitialPlayWithoutUserGestures(true);

    // Do not actually start playback here.
}

Document& AutoplayExperimentHelper::document() const
{
    return element().document();
}

HTMLMediaElement& AutoplayExperimentHelper::element() const
{
    ASSERT(m_element);
    return *m_element;
}

AutoplayExperimentHelper::Mode AutoplayExperimentHelper::fromString(const String& mode)
{
    Mode value = ExperimentOff;
    if (mode.contains("-forvideo"))
        value |= ForVideo;
    if (mode.contains("-foraudio"))
        value |= ForAudio;
    if (mode.contains("-ifpagevisible"))
        value |= IfPageVisible;
    if (mode.contains("-ifviewport"))
        value |= IfViewport;
    if (mode.contains("-ifmuted"))
        value |= IfMuted;
    if (mode.contains("-ifmobile"))
        value |= IfMobile;
    if (mode.contains("-playmuted"))
        value |= PlayMuted;

    return value;
}

} // namespace blink
