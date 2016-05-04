// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/AutoplayExperimentHelper.h"

#include "core/dom/Document.h"
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

AutoplayExperimentHelper::AutoplayExperimentHelper(Client* client)
    : m_client(client)
    , m_mode(Mode::ExperimentOff)
    , m_playPending(false)
    , m_registeredWithLayoutObject(false)
    , m_wasInViewport(false)
    , m_autoplayMediaEncountered(false)
    , m_playbackStartedMetricRecorded(false)
    , m_waitingForAutoplayPlaybackEnd(false)
    , m_recordedElement(false)
    , m_lastLocationUpdateTime(-std::numeric_limits<double>::infinity())
    , m_viewportTimer(this, &AutoplayExperimentHelper::viewportTimerFired)
    , m_autoplayDeferredMetric(GesturelessPlaybackNotOverridden)
{
    m_mode = fromString(this->client().autoplayExperimentMode());

    if (isExperimentEnabled()) {
        WTF_LOG(Media, "HTMLMediaElement: autoplay experiment set to %d",
            m_mode);
    }
}

AutoplayExperimentHelper::~AutoplayExperimentHelper()
{
}

void AutoplayExperimentHelper::dispose()
{
    // Do any cleanup that requires the client.
    unregisterForPositionUpdatesIfNeeded();
}

void AutoplayExperimentHelper::becameReadyToPlay()
{
    // Assuming that we're eligible to override the user gesture requirement,
    // either play if we meet the visibility checks, or install a listener
    // to wait for them to pass.  We do not actually start playback; our
    // caller must do that.
    autoplayMediaEncountered();

    if (isEligible()) {
        if (meetsVisibilityRequirements())
            prepareToAutoplay(GesturelessPlaybackStartedByAutoplayFlagImmediately);
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

    if (!UserGestureIndicator::utilizeUserGesture()) {
        autoplayMediaEncountered();

        if (isEligible()) {
            // Remember that userGestureRequiredForPlay is required for
            // us to be eligible for the experiment.
            // If we are able to override the gesture requirement now, then
            // do so.  Otherwise, install an event listener if we need one.
            if (meetsVisibilityRequirements()) {
                // Override the gesture and assume that play() will succeed.
                prepareToAutoplay(GesturelessPlaybackStartedByPlayMethodImmediately);
            } else {
                // Wait for viewport visibility.
                registerForPositionUpdatesIfNeeded();
            }
        }
    } else if (isUserGestureRequiredForPlay()) {
        // If this media tried to autoplay, and we haven't played it yet, then
        // record that the user provided the gesture to start it the first time.
        if (m_autoplayMediaEncountered && !m_playbackStartedMetricRecorded)
            recordAutoplayMetric(AutoplayManualStart);
        // Don't let future gestureless playbacks affect metrics.
        m_autoplayMediaEncountered = true;
        m_playbackStartedMetricRecorded = true;

        unregisterForPositionUpdatesIfNeeded();
    }
}

void AutoplayExperimentHelper::pauseMethodCalled()
{
    // Don't try to autoplay, if we would have.
    m_playPending = false;
    unregisterForPositionUpdatesIfNeeded();
}

void AutoplayExperimentHelper::loadMethodCalled()
{
    if (isUserGestureRequiredForPlay() && UserGestureIndicator::utilizeUserGesture()) {
        recordAutoplayMetric(AutoplayEnabledThroughLoad);
        removeUserGestureRequirement(GesturelessPlaybackEnabledByLoad);
    }
}

void AutoplayExperimentHelper::mutedChanged()
{
    // If we are no longer eligible for the autoplay experiment, then also
    // quit listening for events.  If we are eligible, and if we should be
    // playing, then start playing.  In other words, start playing if
    // we just needed 'mute' to autoplay.

    // Make sure that autoplay was actually deferred.  If, for example, the
    // autoplay attribute is set after the media is ready to play, then it
    // would normally have no effect.  We don't want to start playing.
    if (!m_autoplayMediaEncountered)
        return;

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
    if (!requiresViewportVisibility()) {
        if (!enabled(IfPageVisible))
            return;
    }

    m_client->setRequestPositionUpdates(true);

    // Set this unconditionally, in case we have no layout object yet.
    m_registeredWithLayoutObject = true;
}

void AutoplayExperimentHelper::unregisterForPositionUpdatesIfNeeded()
{
    if (m_registeredWithLayoutObject)
        m_client->setRequestPositionUpdates(false);

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
    if (visibleRect.isEmpty())
        return;

    m_lastVisibleRect = visibleRect;

    IntRect currentLocation = client().absoluteBoundingBoxRect();
    if (currentLocation.isEmpty())
        return;

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
    if (m_registeredWithLayoutObject)
        m_client->setRequestPositionUpdates(true);
}

void AutoplayExperimentHelper::triggerAutoplayViewportCheckForTesting()
{
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
        && client().pageVisibilityState() != PageVisibilityStateVisible)
        return false;

    if (!requiresViewportVisibility())
        return true;

    if (m_lastVisibleRect.isEmpty())
        return false;

    IntRect currentLocation = client().absoluteBoundingBoxRect();
    if (currentLocation.isEmpty())
        return false;

    // In partial-viewport mode, we require only 1x1 area.
    if (enabled(IfPartialViewport)) {
        return m_lastVisibleRect.intersects(currentLocation);
    }

    // Element must be completely visible, or as much as fits.
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
    prepareToAutoplay(client().shouldAutoplay()
        ? GesturelessPlaybackStartedByAutoplayFlagAfterScroll
        : GesturelessPlaybackStartedByPlayMethodAfterScroll);

    // Record that this played without a user gesture.
    // This should rarely actually do anything.  Usually, playMethodCalled()
    // and becameReadyToPlay will handle it, but toggling muted state can,
    // in some cases, also trigger autoplay if the autoplay attribute is set
    // after the media is ready to play.
    autoplayMediaEncountered();

    client().playInternal();

    return true;
}

bool AutoplayExperimentHelper::isEligible() const
{
    if (m_mode == Mode::ExperimentOff)
        return false;

    // If autoplay is disabled, no one is eligible.
    if (!client().isAutoplayAllowedPerSettings())
        return false;

    // If no user gesture is required, then the experiment doesn't apply.
    // This is what prevents us from starting playback more than once.
    // Since this flag is never set to true once it's cleared, it will block
    // the autoplay experiment forever.
    if (!isUserGestureRequiredForPlay())
        return false;

    // Make sure that this is an element of the right type.
    if (!enabled(ForVideo) && client().isHTMLVideoElement())
        return false;

    if (!enabled(ForAudio) && client().isHTMLAudioElement())
        return false;

    // If nobody has requested playback, either by the autoplay attribute or
    // a play() call, then do nothing.

    if (!m_playPending && !client().shouldAutoplay())
        return false;

    // Note that the viewport test always returns false on desktop, which is
    // why video-autoplay-experiment.html doesn't check -ifmobile .
    if (enabled(IfMobile)
        && !client().isLegacyViewportType())
        return false;

    // If we require same-origin, then check the origin.
    if (enabled(IfSameOrigin) && client().isCrossOrigin())
        return false;

    // If we require muted media and this is muted, then it is eligible.
    if (enabled(IfMuted))
        return client().muted();

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
        client().setMuted(true);
    }
}

void AutoplayExperimentHelper::removeUserGestureRequirement(AutoplayMetrics metric)
{
    if (client().isUserGestureRequiredForPlay()) {
        m_autoplayDeferredMetric = metric;
        client().removeUserGestureRequirement();
    }
}

void AutoplayExperimentHelper::prepareToAutoplay(AutoplayMetrics metric)
{
    // This also causes !isEligible, so that we don't allow autoplay more than
    // once.  Be sure to do this before muteIfNeeded().
    // Also note that, at this point, we know that we're goint to start
    // playback.  However, we still don't record the metric here.  Instead,
    // we let playbackStarted() do that later.
    removeUserGestureRequirement(metric);

    // Don't bother to call autoplayMediaEncountered, since whoever initiates
    // playback has do it anyway, in case we don't allow autoplay.

    unregisterForPositionUpdatesIfNeeded();
    muteIfNeeded();

    // Do not actually start playback here.
}

AutoplayExperimentHelper::Client& AutoplayExperimentHelper::client() const
{
    return *m_client;
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
    if (mode.contains("-ifpartialviewport"))
        value |= IfPartialViewport;
    if (mode.contains("-ifmuted"))
        value |= IfMuted;
    if (mode.contains("-ifmobile"))
        value |= IfMobile;
    if (mode.contains("-ifsameorigin"))
        value |= IfSameOrigin;
    if (mode.contains("-playmuted"))
        value |= PlayMuted;

    return value;
}

void AutoplayExperimentHelper::autoplayMediaEncountered()
{
    if (!m_autoplayMediaEncountered) {
        m_autoplayMediaEncountered = true;
        recordAutoplayMetric(AutoplayMediaFound);
    }
}

bool AutoplayExperimentHelper::isUserGestureRequiredForPlay() const
{
    return client().isUserGestureRequiredForPlay();
}

void AutoplayExperimentHelper::playbackStarted()
{
    recordAutoplayMetric(AnyPlaybackStarted);

    if (m_playbackStartedMetricRecorded)
        return;

    m_playbackStartedMetricRecorded = true;

    // If this is a gestureless start, record why it was allowed.
    if (!UserGestureIndicator::processingUserGesture()) {
        m_waitingForAutoplayPlaybackEnd = true;
        recordAutoplayMetric(m_autoplayDeferredMetric);
    }
}

void AutoplayExperimentHelper::playbackStopped()
{
    const bool ended = client().ended();
    const bool bailout = isBailout();

    // Record that play was paused.  We don't care if it was autoplay,
    // play(), or the user manually started it.
    recordAutoplayMetric(ended ? AnyPlaybackComplete : AnyPlaybackPaused);
    if (bailout)
        recordAutoplayMetric(AnyPlaybackBailout);

    // If this was a gestureless play, then record that separately.
    // These cover attr and play() gestureless starts.
    if (m_waitingForAutoplayPlaybackEnd) {
        m_waitingForAutoplayPlaybackEnd = false;

        recordAutoplayMetric(ended ? AutoplayComplete : AutoplayPaused);

        if (bailout)
            recordAutoplayMetric(AutoplayBailout);
    }
}

void AutoplayExperimentHelper::recordAutoplayMetric(AutoplayMetrics metric)
{
    client().recordAutoplayMetric(metric);
}

bool AutoplayExperimentHelper::isBailout() const
{
    // We count the user as having bailed-out on the video if they watched
    // less than one minute and less than 50% of it.
    const double playedTime = client().currentTime();
    const double progress = playedTime / client().duration();
    return (playedTime < 60) && (progress < 0.5);
}

void AutoplayExperimentHelper::recordSandboxFailure()
{
    // We record autoplayMediaEncountered here because we know
    // that the autoplay attempt will fail.
    autoplayMediaEncountered();
    recordAutoplayMetric(AutoplayDisabledBySandbox);
}

void AutoplayExperimentHelper::loadingStarted()
{
    if (m_recordedElement)
        return;

    m_recordedElement = true;
    recordAutoplayMetric(client().isHTMLVideoElement()
        ? AnyVideoElement
        : AnyAudioElement);
}

bool AutoplayExperimentHelper::requiresViewportVisibility() const
{
    return client().isHTMLVideoElement() && (enabled(IfViewport) || enabled(IfPartialViewport));
}

bool AutoplayExperimentHelper::isExperimentEnabled()
{
    return m_mode != Mode::ExperimentOff;
}

} // namespace blink
