// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
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

AutoplayExperimentHelper::AutoplayExperimentHelper(HTMLMediaElement& element)
    : m_element(element)
    , m_mode(AutoplayExperimentConfig::Mode::Off)
    , m_playPending(false)
{
    if (document().settings()) {
        m_mode = AutoplayExperimentConfig::fromString(document().settings()->autoplayExperimentMode());

        if (m_mode != AutoplayExperimentConfig::Mode::Off) {
            WTF_LOG(Media, "HTMLMediaElement: autoplay experiment set to %d",
                m_mode);
        }
    }
}

AutoplayExperimentHelper::~AutoplayExperimentHelper()
{
}

void AutoplayExperimentHelper::becameReadyToPlay()
{
    // Assuming that we're eligible to override the user gesture requirement,
    // then play.
    if (isEligible()) {
        prepareToPlay(GesturelessPlaybackStartedByAutoplayFlagImmediately);
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
            // We are able to override the gesture requirement now, so
            // do so.
            prepareToPlay(GesturelessPlaybackStartedByPlayMethodImmediately);
        }

    }
}

void AutoplayExperimentHelper::pauseMethodCalled()
{
    // Don't try to autoplay, if we would have.
    m_playPending = false;
}

void AutoplayExperimentHelper::mutedChanged()
{
    // In other words, start playing if we just needed 'mute' to autoplay.
    maybeStartPlaying();
}

bool AutoplayExperimentHelper::maybeStartPlaying()
{
    // See if we're allowed to autoplay now.
    if (!isEligible()) {
        return false;
    }

    // Start playing!
    prepareToPlay(m_element.shouldAutoplay()
        ? GesturelessPlaybackStartedByAutoplayFlagAfterScroll
        : GesturelessPlaybackStartedByPlayMethodAfterScroll);
    m_element.playInternal();

    return true;
}

bool AutoplayExperimentHelper::isEligible() const
{
    // If no user gesture is required, then the experiment doesn't apply.
    // This is what prevents us from starting playback more than once.
    // Since this flag is never set to true once it's cleared, it will block
    // the autoplay experiment forever.
    if (!m_element.isUserGestureRequiredForPlay())
        return false;

    if (m_mode == AutoplayExperimentConfig::Mode::Off)
        return false;

    // Make sure that this is an element of the right type.
    if (!enabled(AutoplayExperimentConfig::Mode::ForVideo)
        && isHTMLVideoElement(m_element))
        return false;

    if (!enabled(AutoplayExperimentConfig::Mode::ForAudio)
        && isHTMLAudioElement(m_element))
        return false;

    // If nobody has requested playback, either by the autoplay attribute or
    // a play() call, then do nothing.
    if (!m_playPending && !m_element.shouldAutoplay())
        return false;

    // If the video is already playing, then do nothing.  Note that there
    // is not a path where a user gesture is required but the video is
    // playing.  However, we check for completeness.
    if (!m_element.paused())
        return false;

    // Note that the viewport test always returns false on desktop, which is
    // why video-autoplay-experiment.html doesn't check -ifmobile .
    if (enabled(AutoplayExperimentConfig::Mode::IfMobile)
        && !document().viewportDescription().isLegacyViewportType())
        return false;

    // If media is muted, then autoplay when it comes into view.
    if (enabled(AutoplayExperimentConfig::Mode::IfMuted))
        return m_element.muted();

    // Autoplay when it comes into view (if needed), maybe muted.
    return true;
}

void AutoplayExperimentHelper::muteIfNeeded()
{
    if (enabled(AutoplayExperimentConfig::Mode::PlayMuted)) {
        ASSERT(!isEligible());
        // If we are actually changing the muted state, then this will call
        // mutedChanged().  If isEligible(), then mutedChanged() will try
        // to start playback, which we should not do here.
        m_element.setMuted(true);
    }
}

void AutoplayExperimentHelper::prepareToPlay(AutoplayMetrics metric)
{
    m_element.recordAutoplayMetric(metric);

    // This also causes !isEligible, so that we don't allow autoplay more than
    // once.  Be sure to do this before muteIfNeeded().
    m_element.removeUserGestureRequirement();

    muteIfNeeded();

    // Record that this autoplayed without a user gesture.  This is normally
    // set when we discover an autoplay attribute, but we include all cases
    // where playback started without a user gesture, e.g., play().
    m_element.setInitialPlayWithoutUserGestures(true);

    // Do not actually start playback here.
}

Document& AutoplayExperimentHelper::document() const
{
    return m_element.document();
}

}
