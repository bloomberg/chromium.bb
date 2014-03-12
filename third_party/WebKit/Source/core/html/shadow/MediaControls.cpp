/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/html/shadow/MediaControls.h"

#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/events/MouseEvent.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/MediaController.h"
#include "core/rendering/RenderTheme.h"

namespace WebCore {

#if OS(ANDROID)
static const bool alwaysHideFullscreenControls = true;
static const bool needOverlayPlayButton = true;
#else
static const bool alwaysHideFullscreenControls = false;
static const bool needOverlayPlayButton = false;
#endif

static const double timeWithoutMouseMovementBeforeHidingFullscreenControls = 3;

MediaControls::MediaControls(HTMLMediaElement& mediaElement)
    : HTMLDivElement(mediaElement.document())
    , m_mediaElement(mediaElement)
    , m_panel(0)
    , m_textDisplayContainer(0)
    , m_overlayPlayButton(0)
    , m_overlayEnclosure(0)
    , m_playButton(0)
    , m_currentTimeDisplay(0)
    , m_timeline(0)
    , m_muteButton(0)
    , m_volumeSlider(0)
    , m_toggleClosedCaptionsButton(0)
    , m_fullScreenButton(0)
    , m_durationDisplay(0)
    , m_enclosure(0)
    , m_hideFullscreenControlsTimer(this, &MediaControls::hideFullscreenControlsTimerFired)
    , m_isFullscreen(false)
    , m_isMouseOverControls(false)
{
}

PassRefPtr<MediaControls> MediaControls::create(HTMLMediaElement& mediaElement)
{
    RefPtr<MediaControls> controls = adoptRef(new MediaControls(mediaElement));

    if (controls->initializeControls())
        return controls.release();

    return nullptr;
}

bool MediaControls::initializeControls()
{
    TrackExceptionState exceptionState;

    if (needOverlayPlayButton) {
        RefPtr<MediaControlOverlayEnclosureElement> overlayEnclosure = MediaControlOverlayEnclosureElement::create(*this);
        RefPtr<MediaControlOverlayPlayButtonElement> overlayPlayButton = MediaControlOverlayPlayButtonElement::create(*this);
        m_overlayPlayButton = overlayPlayButton.get();
        overlayEnclosure->appendChild(overlayPlayButton.release(), exceptionState);
        if (exceptionState.hadException())
            return false;

        m_overlayEnclosure = overlayEnclosure.get();
        appendChild(overlayEnclosure.release(), exceptionState);
        if (exceptionState.hadException())
            return false;
    }

    // Create an enclosing element for the panel so we can visually offset the controls correctly.
    RefPtr<MediaControlPanelEnclosureElement> enclosure = MediaControlPanelEnclosureElement::create(*this);

    RefPtr<MediaControlPanelElement> panel = MediaControlPanelElement::create(*this);

    RefPtr<MediaControlPlayButtonElement> playButton = MediaControlPlayButtonElement::create(*this);
    m_playButton = playButton.get();
    panel->appendChild(playButton.release(), exceptionState);
    if (exceptionState.hadException())
        return false;

    RefPtr<MediaControlTimelineElement> timeline = MediaControlTimelineElement::create(*this);
    m_timeline = timeline.get();
    panel->appendChild(timeline.release(), exceptionState);
    if (exceptionState.hadException())
        return false;

    RefPtr<MediaControlCurrentTimeDisplayElement> currentTimeDisplay = MediaControlCurrentTimeDisplayElement::create(*this);
    m_currentTimeDisplay = currentTimeDisplay.get();
    m_currentTimeDisplay->hide();
    panel->appendChild(currentTimeDisplay.release(), exceptionState);
    if (exceptionState.hadException())
        return false;

    RefPtr<MediaControlTimeRemainingDisplayElement> durationDisplay = MediaControlTimeRemainingDisplayElement::create(*this);
    m_durationDisplay = durationDisplay.get();
    panel->appendChild(durationDisplay.release(), exceptionState);
    if (exceptionState.hadException())
        return false;

    RefPtr<MediaControlMuteButtonElement> muteButton = MediaControlMuteButtonElement::create(*this);
    m_muteButton = muteButton.get();
    panel->appendChild(muteButton.release(), exceptionState);
    if (exceptionState.hadException())
        return false;

    RefPtr<MediaControlVolumeSliderElement> slider = MediaControlVolumeSliderElement::create(*this);
    m_volumeSlider = slider.get();
    panel->appendChild(slider.release(), exceptionState);
    if (exceptionState.hadException())
        return false;

    RefPtr<MediaControlToggleClosedCaptionsButtonElement> toggleClosedCaptionsButton = MediaControlToggleClosedCaptionsButtonElement::create(*this);
    m_toggleClosedCaptionsButton = toggleClosedCaptionsButton.get();
    panel->appendChild(toggleClosedCaptionsButton.release(), exceptionState);
    if (exceptionState.hadException())
        return false;

    RefPtr<MediaControlFullscreenButtonElement> fullscreenButton = MediaControlFullscreenButtonElement::create(*this);
    m_fullScreenButton = fullscreenButton.get();
    panel->appendChild(fullscreenButton.release(), exceptionState);
    if (exceptionState.hadException())
        return false;

    m_panel = panel.get();
    enclosure->appendChild(panel.release(), exceptionState);
    if (exceptionState.hadException())
        return false;

    m_enclosure = enclosure.get();
    appendChild(enclosure.release(), exceptionState);
    if (exceptionState.hadException())
        return false;

    return true;
}

MediaControllerInterface& MediaControls::mediaControllerInterface() const
{
    if (m_mediaElement.controller())
        return *m_mediaElement.controller();
    return m_mediaElement;
}

void MediaControls::reset()
{
    double duration = mediaControllerInterface().duration();
    m_durationDisplay->setInnerText(RenderTheme::theme().formatMediaControlsTime(duration), ASSERT_NO_EXCEPTION);
    m_durationDisplay->setCurrentValue(duration);

    m_playButton->updateDisplayType();

    updateCurrentTimeDisplay();

    m_timeline->setDuration(mediaControllerInterface().duration());
    m_timeline->setPosition(mediaControllerInterface().currentTime());

    if (!mediaControllerInterface().hasAudio()) {
        m_volumeSlider->hide();
    } else {
        m_volumeSlider->show();
        m_volumeSlider->setVolume(mediaControllerInterface().volume());
    }

    refreshClosedCaptionsButtonVisibility();

    if (mediaControllerInterface().hasVideo())
        m_fullScreenButton->show();
    else
        m_fullScreenButton->hide();

    makeOpaque();
}

void MediaControls::show()
{
    makeOpaque();
    m_panel->setIsDisplayed(true);
    m_panel->show();
}

void MediaControls::hide()
{
    m_panel->setIsDisplayed(false);
    m_panel->hide();
}

void MediaControls::makeOpaque()
{
    m_panel->makeOpaque();
}

void MediaControls::makeTransparent()
{
    m_panel->makeTransparent();
}

bool MediaControls::shouldHideFullscreenControls()
{
    return alwaysHideFullscreenControls || !m_panel->hovered();
}

void MediaControls::playbackStarted()
{
    m_currentTimeDisplay->show();
    m_durationDisplay->hide();

    if (m_overlayPlayButton)
        m_overlayPlayButton->updateDisplayType();
    m_playButton->updateDisplayType();
    m_timeline->setPosition(mediaControllerInterface().currentTime());
    updateCurrentTimeDisplay();

    if (m_isFullscreen)
        startHideFullscreenControlsTimer();
}

void MediaControls::playbackProgressed()
{
    m_timeline->setPosition(mediaControllerInterface().currentTime());
    updateCurrentTimeDisplay();

    if (!m_isMouseOverControls && mediaControllerInterface().hasVideo())
        makeTransparent();
}

void MediaControls::playbackStopped()
{
    if (m_overlayPlayButton)
        m_overlayPlayButton->updateDisplayType();
    m_playButton->updateDisplayType();
    m_timeline->setPosition(mediaControllerInterface().currentTime());
    updateCurrentTimeDisplay();
    makeOpaque();

    stopHideFullscreenControlsTimer();
}

void MediaControls::updateCurrentTimeDisplay()
{
    double now = mediaControllerInterface().currentTime();
    double duration = mediaControllerInterface().duration();

    // After seek, hide duration display and show current time.
    if (now > 0) {
        m_currentTimeDisplay->show();
        m_durationDisplay->hide();
    }

    // Allow the theme to format the time.
    m_currentTimeDisplay->setInnerText(RenderTheme::theme().formatMediaControlsCurrentTime(now, duration), IGNORE_EXCEPTION);
    m_currentTimeDisplay->setCurrentValue(now);
}

void MediaControls::changedMute()
{
    m_muteButton->updateDisplayType();

    if (mediaControllerInterface().muted())
        m_volumeSlider->setVolume(0);
    else
        m_volumeSlider->setVolume(mediaControllerInterface().volume());
}

void MediaControls::changedVolume()
{
    m_volumeSlider->setVolume(mediaControllerInterface().volume());
    if (m_muteButton->renderer())
        m_muteButton->renderer()->repaint();
}

void MediaControls::changedClosedCaptionsVisibility()
{
    m_toggleClosedCaptionsButton->updateDisplayType();
}

void MediaControls::refreshClosedCaptionsButtonVisibility()
{
    if (mediaControllerInterface().hasClosedCaptions())
        m_toggleClosedCaptionsButton->show();
    else
        m_toggleClosedCaptionsButton->hide();
}

void MediaControls::closedCaptionTracksChanged()
{
    refreshClosedCaptionsButtonVisibility();
}

void MediaControls::enteredFullscreen()
{
    m_isFullscreen = true;
    m_fullScreenButton->setIsFullscreen(true);
    startHideFullscreenControlsTimer();
}

void MediaControls::exitedFullscreen()
{
    m_isFullscreen = false;
    m_fullScreenButton->setIsFullscreen(false);
    stopHideFullscreenControlsTimer();
}

void MediaControls::defaultEventHandler(Event* event)
{
    HTMLDivElement::defaultEventHandler(event);

    if (event->type() == EventTypeNames::mouseover) {
        if (!containsRelatedTarget(event)) {
            m_isMouseOverControls = true;
            if (!mediaControllerInterface().canPlay()) {
                makeOpaque();
                if (shouldHideFullscreenControls())
                    startHideFullscreenControlsTimer();
            }
        }
        return;
    }

    if (event->type() == EventTypeNames::mouseout) {
        if (!containsRelatedTarget(event)) {
            m_isMouseOverControls = false;
            stopHideFullscreenControlsTimer();
        }
        return;
    }

    if (event->type() == EventTypeNames::mousemove) {
        if (m_isFullscreen) {
            // When we get a mouse move in fullscreen mode, show the media controls, and start a timer
            // that will hide the media controls after a 3 seconds without a mouse move.
            makeOpaque();
            if (shouldHideFullscreenControls())
                startHideFullscreenControlsTimer();
        }
        return;
    }
}

void MediaControls::hideFullscreenControlsTimerFired(Timer<MediaControls>*)
{
    if (mediaControllerInterface().paused())
        return;

    if (!m_isFullscreen)
        return;

    if (!shouldHideFullscreenControls())
        return;

    makeTransparent();
}

void MediaControls::startHideFullscreenControlsTimer()
{
    if (!m_isFullscreen)
        return;

    m_hideFullscreenControlsTimer.startOneShot(timeWithoutMouseMovementBeforeHidingFullscreenControls, FROM_HERE);
}

void MediaControls::stopHideFullscreenControlsTimer()
{
    m_hideFullscreenControlsTimer.stop();
}

const AtomicString& MediaControls::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls"));
    return id;
}

bool MediaControls::containsRelatedTarget(Event* event)
{
    if (!event->isMouseEvent())
        return false;
    EventTarget* relatedTarget = toMouseEvent(event)->relatedTarget();
    if (!relatedTarget)
        return false;
    return contains(relatedTarget->toNode());
}

void MediaControls::createTextTrackDisplay()
{
    if (m_textDisplayContainer)
        return;

    RefPtr<MediaControlTextTrackContainerElement> textDisplayContainer = MediaControlTextTrackContainerElement::create(*this);
    m_textDisplayContainer = textDisplayContainer.get();

    // Insert it before (behind) all other control elements.
    if (m_overlayEnclosure && m_overlayPlayButton)
        m_overlayEnclosure->insertBefore(textDisplayContainer.release(), m_overlayPlayButton);
    else
        insertBefore(textDisplayContainer.release(), m_enclosure);
}

void MediaControls::showTextTrackDisplay()
{
    if (!m_textDisplayContainer)
        createTextTrackDisplay();
    m_textDisplayContainer->show();
}

void MediaControls::hideTextTrackDisplay()
{
    if (!m_textDisplayContainer)
        createTextTrackDisplay();
    m_textDisplayContainer->hide();
}

void MediaControls::updateTextTrackDisplay()
{
    if (!m_textDisplayContainer)
        createTextTrackDisplay();

    m_textDisplayContainer->updateDisplay();
}

}
