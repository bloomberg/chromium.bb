// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AutoplayExperimentHelper_h
#define AutoplayExperimentHelper_h

#include "core/html/AutoplayExperimentConfig.h"
#include "core/page/Page.h"
#include "platform/Timer.h"
#include "platform/geometry/IntRect.h"

namespace blink {
class Document;
class HTMLMediaElement;
class EventListener;

// These values are used for a histogram. Do not reorder.
enum AutoplayMetrics {
    // Media element with autoplay seen.
    AutoplayMediaFound = 0,
    // Autoplay enabled and user stopped media play at any point.
    AutoplayPaused = 1,
    // Autoplay enabled but user bailed out on media play early.
    AutoplayBailout = 2,
    // Autoplay disabled but user manually started media.
    AutoplayManualStart = 3,
    // Autoplay was (re)enabled through a user-gesture triggered load()
    AutoplayEnabledThroughLoad = 4,
    // Autoplay disabled by sandbox flags.
    AutoplayDisabledBySandbox = 5,

    // These metrics indicate "no gesture detected, but the gesture
    // requirement was overridden by experiment".  They do not include cases
    // where no user gesture is required (!m_userGestureRequiredForPlay).

    // User gesture requirement was bypassed, and playback started, during
    // initial load via the autoplay flag.  If playback was deferred due to
    // visibility requirements, then this does not apply.
    GesturelessPlaybackStartedByAutoplayFlagImmediately = 6,

    // User gesture requirement was bypassed, and playback started, when
    // the play() method was called.  If playback is deferred due to
    // visibility requirements, then this does not apply.
    GesturelessPlaybackStartedByPlayMethodImmediately = 7,

    // User gesture requirement was bypassed, and playback started, after
    // an element with the autoplay flag moved into the viewport.  Playback
    // was deferred earlier due to visibility requirements.
    GesturelessPlaybackStartedByAutoplayFlagAfterScroll = 8,

    // User gesture requirement was bypassed, and playback started, after
    // an element on which the play() method was called was moved into the
    // viewport.  Playback had been deferred due to visibility requirements.
    GesturelessPlaybackStartedByPlayMethodAfterScroll = 9,

    // play() failed to play due to gesture requirement.
    PlayMethodFailed = 10,

    // Some play, whether user initiated or not, started.
    AnyPlaybackStarted = 11,
    // Some play, whether user initiated or not, paused.
    AnyPlaybackPaused = 12,
    // Some playback, whether user initiated or not, bailed out early.
    AnyPlaybackBailout = 13,

    // This enum value must be last.
    NumberOfAutoplayMetrics,
};

class AutoplayExperimentHelper {
public:
    AutoplayExperimentHelper(HTMLMediaElement&);
    ~AutoplayExperimentHelper();

    void becameReadyToPlay();
    void playMethodCalled();
    void pauseMethodCalled();
    void mutedChanged();
    void positionChanged();

private:
    // Return true if any only if this player meets (most) of the eligibility
    // requirements for the experiment to override the need for a user
    // gesture.  This includes everything except the visibility test.
    bool isEligible() const;

    // Set the muted flag on the media if we're in an experiment mode that
    // requires it, else do nothing.
    void muteIfNeeded();

    // Maybe override the requirement for a user gesture, and start playing
    // autoplay media.  Returns true if only if it starts playback.
    bool maybeStartPlaying();

    // Configure internal state to record that the autoplay experiment is
    // going to start playback.  This doesn't actually start playback, since
    // there are several different cases.
    void prepareToPlay(AutoplayMetrics);

    // Return our media element's document.
    Document& document() const;

    inline bool enabled(AutoplayExperimentConfig::Mode mode) const
    {
        return ((int)m_mode) & ((int)mode);
    }

private:
    HTMLMediaElement& m_element;

    AutoplayExperimentConfig::Mode m_mode;

    // Autoplay experiment state.
    // True if we've received a play() without a pause().
    bool m_playPending : 1;

    friend class Internals;
};

} // namespace blink

#endif // AutoplayExperimentHelper_h
