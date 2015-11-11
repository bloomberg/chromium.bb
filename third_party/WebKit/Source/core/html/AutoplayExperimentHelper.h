// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AutoplayExperimentHelper_h
#define AutoplayExperimentHelper_h

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

class AutoplayExperimentHelper final {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
public:
    explicit AutoplayExperimentHelper(HTMLMediaElement&);
    ~AutoplayExperimentHelper();

    void becameReadyToPlay();
    void playMethodCalled();
    void pauseMethodCalled();
    void mutedChanged();
    void positionChanged(const IntRect&);
    void updatePositionNotificationRegistration();

    void triggerAutoplayViewportCheckForTesting();

    enum Mode {
        // Do not enable the autoplay experiment.
        ExperimentOff = 0,
        // Enable gestureless autoplay for video elements.
        ForVideo      = 1 << 0,
        // Enable gestureless autoplay for audio elements.
        ForAudio      = 1 << 1,
        // Restrict gestureless autoplay to media that is in a visible page.
        IfPageVisible = 1 << 2,
        // Restrict gestureless autoplay to media that is visible in
        // the viewport.
        IfViewport    = 1 << 3,
        // Restrict gestureless autoplay to audio-less or muted media.
        IfMuted       = 1 << 4,
        // Restrict gestureless autoplay to sites which contain the
        // viewport tag.
        IfMobile      = 1 << 5,
        // If gestureless autoplay is allowed, then mute the media before
        // starting to play.
        PlayMuted     = 1 << 6,
    };

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_element);
    }

private:
    // Register to receive position updates, if we haven't already.  If we
    // have, then this does nothing.
    void registerForPositionUpdatesIfNeeded();

    // Un-register for position updates, if we are currently registered.
    void unregisterForPositionUpdatesIfNeeded();

    // Return true if any only if this player meets (most) of the eligibility
    // requirements for the experiment to override the need for a user
    // gesture.  This includes everything except the visibility test.
    bool isEligible() const;

    // Return false if and only if m_element is not visible, and we care
    // that it must be visible.
    bool meetsVisibilityRequirements() const;

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

    // Process a timer for checking visibility.
    void viewportTimerFired(Timer<AutoplayExperimentHelper>*);

    // Return our media element's document.
    Document& document() const;

    HTMLMediaElement& element() const;

    inline bool enabled(Mode mode) const
    {
        return ((int)m_mode) & ((int)mode);
    }

    Mode fromString(const String& mode);

    RawPtrWillBeMember<HTMLMediaElement> m_element;

    Mode m_mode;

    // Autoplay experiment state.
    // True if we've received a play() without a pause().
    bool m_playPending : 1;

    // Are we registered with the view for position updates?
    bool m_registeredWithLayoutObject : 1;

    // According to our last position update, are we in the viewport?
    bool m_wasInViewport : 1;

    // According to our last position update, where was our element?
    IntRect m_lastLocation;
    IntRect m_lastVisibleRect;

    // When was m_lastLocation set?
    double m_lastLocationUpdateTime;

    Timer<AutoplayExperimentHelper> m_viewportTimer;
};

inline AutoplayExperimentHelper::Mode& operator|=(AutoplayExperimentHelper::Mode& a,
    const AutoplayExperimentHelper::Mode& b)
{
    a = static_cast<AutoplayExperimentHelper::Mode>(static_cast<int>(a) | static_cast<int>(b));
    return a;
}


} // namespace blink

#endif // AutoplayExperimentHelper_h
