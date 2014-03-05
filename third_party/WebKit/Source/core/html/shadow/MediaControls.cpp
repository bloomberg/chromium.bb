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
#if OS(ANDROID)
#include "core/html/shadow/MediaControlsAndroid.h"
#endif

namespace WebCore {

static const double timeWithoutMouseMovementBeforeHidingFullscreenControls = 3;

MediaControls::MediaControls(Document& document)
    : HTMLDivElement(document)
    , m_mediaController(0)
    , m_panel(0)
    , m_textDisplayContainer(0)
    , m_playButton(0)
    , m_currentTimeDisplay(0)
    , m_timeline(0)
    , m_panelMuteButton(0)
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

PassRefPtr<MediaControls> MediaControls::create(Document& document)
{
    if (!document.page())
        return nullptr;

    RefPtr<MediaControls> controls;
#if OS(ANDROID)
    controls = adoptRef(new MediaControlsAndroid(document));
#else
    controls = adoptRef(new MediaControls(document));
#endif

    if (controls->initializeControls(document))
        return controls.release();

    return nullptr;
}

bool MediaControls::initializeControls(Document& document)
{
    // Create an enclosing element for the panel so we can visually offset the controls correctly.
    RefPtr<MediaControlPanelEnclosureElement> enclosure = MediaControlPanelEnclosureElement::create(document);

    RefPtr<MediaControlPanelElement> panel = MediaControlPanelElement::create(document);

    TrackExceptionState exceptionState;

    RefPtr<MediaControlPlayButtonElement> playButton = MediaControlPlayButtonElement::create(document);
    m_playButton = playButton.get();
    panel->appendChild(playButton.release(), exceptionState);
    if (exceptionState.hadException())
        return false;

    RefPtr<MediaControlTimelineElement> timeline = MediaControlTimelineElement::create(document, this);
    m_timeline = timeline.get();
    panel->appendChild(timeline.release(), exceptionState);
    if (exceptionState.hadException())
        return false;

    RefPtr<MediaControlCurrentTimeDisplayElement> currentTimeDisplay = MediaControlCurrentTimeDisplayElement::create(document);
    m_currentTimeDisplay = currentTimeDisplay.get();
    m_currentTimeDisplay->hide();
    panel->appendChild(currentTimeDisplay.release(), exceptionState);
    if (exceptionState.hadException())
        return false;

    RefPtr<MediaControlTimeRemainingDisplayElement> durationDisplay = MediaControlTimeRemainingDisplayElement::create(document);
    m_durationDisplay = durationDisplay.get();
    panel->appendChild(durationDisplay.release(), exceptionState);
    if (exceptionState.hadException())
        return false;

    RefPtr<MediaControlPanelMuteButtonElement> panelMuteButton = MediaControlPanelMuteButtonElement::create(document, this);
    m_panelMuteButton = panelMuteButton.get();
    panel->appendChild(panelMuteButton.release(), exceptionState);
    if (exceptionState.hadException())
        return false;

    RefPtr<MediaControlPanelVolumeSliderElement> slider = MediaControlPanelVolumeSliderElement::create(document);
    m_volumeSlider = slider.get();
    panel->appendChild(slider.release(), exceptionState);
    if (exceptionState.hadException())
        return false;

    RefPtr<MediaControlToggleClosedCaptionsButtonElement> toggleClosedCaptionsButton = MediaControlToggleClosedCaptionsButtonElement::create(document, this);
    m_toggleClosedCaptionsButton = toggleClosedCaptionsButton.get();
    panel->appendChild(toggleClosedCaptionsButton.release(), exceptionState);
    if (exceptionState.hadException())
        return false;

    RefPtr<MediaControlFullscreenButtonElement> fullscreenButton = MediaControlFullscreenButtonElement::create(document);
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

void MediaControls::setMediaController(MediaControllerInterface* controller)
{
    if (m_mediaController == controller)
        return;
    m_mediaController = controller;

    if (m_panel)
        m_panel->setMediaController(controller);
    if (m_textDisplayContainer)
        m_textDisplayContainer->setMediaController(controller);
    if (m_playButton)
        m_playButton->setMediaController(controller);
    if (m_currentTimeDisplay)
        m_currentTimeDisplay->setMediaController(controller);
    if (m_timeline)
        m_timeline->setMediaController(controller);
    if (m_panelMuteButton)
        m_panelMuteButton->setMediaController(controller);
    if (m_volumeSlider)
        m_volumeSlider->setMediaController(controller);
    if (m_toggleClosedCaptionsButton)
        m_toggleClosedCaptionsButton->setMediaController(controller);
    if (m_fullScreenButton)
        m_fullScreenButton->setMediaController(controller);
    if (m_durationDisplay)
        m_durationDisplay->setMediaController(controller);
    if (m_enclosure)
        m_enclosure->setMediaController(controller);
}

void MediaControls::reset()
{
    Page* page = document().page();
    if (!page)
        return;

    double duration = m_mediaController->duration();
    m_durationDisplay->setInnerText(RenderTheme::theme().formatMediaControlsTime(duration), ASSERT_NO_EXCEPTION);
    m_durationDisplay->setCurrentValue(duration);

    m_playButton->updateDisplayType();

    updateCurrentTimeDisplay();

    m_timeline->setDuration(m_mediaController->duration());
    m_timeline->setPosition(m_mediaController->currentTime());

    m_panelMuteButton->show();

    if (m_volumeSlider) {
        if (!m_mediaController->hasAudio())
            m_volumeSlider->hide();
        else {
            m_volumeSlider->show();
            m_volumeSlider->setVolume(m_mediaController->volume());
        }
    }

    refreshClosedCaptionsButtonVisibility();

    if (m_fullScreenButton) {
        if (m_mediaController->hasVideo())
            m_fullScreenButton->show();
        else
            m_fullScreenButton->hide();
    }

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

bool MediaControls::shouldHideControls()
{
    return !m_panel->hovered();
}

void MediaControls::bufferingProgressed()
{
    // We only need to update buffering progress when paused, during normal
    // playback playbackProgressed() will take care of it.
    if (m_mediaController->paused())
        m_timeline->setPosition(m_mediaController->currentTime());
}

void MediaControls::playbackStarted()
{
    m_currentTimeDisplay->show();
    m_durationDisplay->hide();

    m_playButton->updateDisplayType();
    m_timeline->setPosition(m_mediaController->currentTime());
    updateCurrentTimeDisplay();

    if (m_isFullscreen)
        startHideFullscreenControlsTimer();
}

void MediaControls::playbackProgressed()
{
    m_timeline->setPosition(m_mediaController->currentTime());
    updateCurrentTimeDisplay();

    if (!m_isMouseOverControls && m_mediaController->hasVideo())
        makeTransparent();
}

void MediaControls::playbackStopped()
{
    m_playButton->updateDisplayType();
    m_timeline->setPosition(m_mediaController->currentTime());
    updateCurrentTimeDisplay();
    makeOpaque();

    stopHideFullscreenControlsTimer();
}

void MediaControls::updateCurrentTimeDisplay()
{
    double now = m_mediaController->currentTime();
    double duration = m_mediaController->duration();

    Page* page = document().page();
    if (!page)
        return;

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
    m_panelMuteButton->changedMute();

    if (m_mediaController->muted())
        m_volumeSlider->setVolume(0);
    else
        m_volumeSlider->setVolume(m_mediaController->volume());
}

void MediaControls::changedVolume()
{
    if (m_volumeSlider)
        m_volumeSlider->setVolume(m_mediaController->volume());
    if (m_panelMuteButton && m_panelMuteButton->renderer())
        m_panelMuteButton->renderer()->repaint();
}

void MediaControls::changedClosedCaptionsVisibility()
{
    if (m_toggleClosedCaptionsButton)
        m_toggleClosedCaptionsButton->updateDisplayType();
}

void MediaControls::refreshClosedCaptionsButtonVisibility()
{
    if (!m_toggleClosedCaptionsButton)
        return;

    if (m_mediaController->hasClosedCaptions())
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
            if (!m_mediaController->canPlay()) {
                makeOpaque();
                if (shouldHideControls())
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
            if (shouldHideControls())
                startHideFullscreenControlsTimer();
        }
        return;
    }
}

void MediaControls::hideFullscreenControlsTimerFired(Timer<MediaControls>*)
{
    if (m_mediaController->paused())
        return;

    if (!m_isFullscreen)
        return;

    if (!shouldHideControls())
        return;

    makeTransparent();
}

void MediaControls::startHideFullscreenControlsTimer()
{
    if (!m_isFullscreen)
        return;

    Page* page = document().page();
    if (!page)
        return;

    m_hideFullscreenControlsTimer.startOneShot(timeWithoutMouseMovementBeforeHidingFullscreenControls);
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

    RefPtr<MediaControlTextTrackContainerElement> textDisplayContainer = MediaControlTextTrackContainerElement::create(document());
    m_textDisplayContainer = textDisplayContainer.get();

    if (m_mediaController)
        m_textDisplayContainer->setMediaController(m_mediaController);

    insertTextTrackContainer(textDisplayContainer.release());
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

void MediaControls::insertTextTrackContainer(PassRefPtr<MediaControlTextTrackContainerElement> textTrackContainer)
{
    // Insert it before the first controller element so it always displays behind the controls.
    insertBefore(textTrackContainer, m_enclosure);
}

}
