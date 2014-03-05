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

#ifndef MediaControls_h
#define MediaControls_h

#include "core/events/MouseEvent.h"
#include "core/html/HTMLDivElement.h"
#include "core/html/shadow/MediaControlElements.h"
#include "core/rendering/RenderTheme.h"

namespace WebCore {

class Document;
class Event;
class MediaPlayer;

class RenderBox;
class RenderMedia;

class MediaControls : public HTMLDivElement {
public:
    virtual ~MediaControls() {}

    static PassRefPtr<MediaControls> create(Document&);

    virtual void setMediaController(MediaControllerInterface*);

    void reset();

    void show();
    void hide();

    void bufferingProgressed();
    virtual void playbackStarted();
    void playbackProgressed();
    virtual void playbackStopped();

    void updateCurrentTimeDisplay();

    void changedMute();
    void changedVolume();

    void changedClosedCaptionsVisibility();
    void refreshClosedCaptionsButtonVisibility();
    void closedCaptionTracksChanged();

    void enteredFullscreen();
    void exitedFullscreen();

    void updateTextTrackDisplay();

protected:
    explicit MediaControls(Document&);

    virtual bool initializeControls(Document&);

    virtual bool shouldHideControls();

    virtual void insertTextTrackContainer(PassRefPtr<MediaControlTextTrackContainerElement>);

private:
    void makeOpaque();
    void makeTransparent();

    void hideFullscreenControlsTimerFired(Timer<MediaControls>*);
    void startHideFullscreenControlsTimer();
    void stopHideFullscreenControlsTimer();

    void createTextTrackDisplay();
    void showTextTrackDisplay();
    void hideTextTrackDisplay();

    // Node
    virtual bool isMediaControls() const OVERRIDE FINAL { return true; }
    virtual bool willRespondToMouseMoveEvents() OVERRIDE { return true; }
    virtual void defaultEventHandler(Event*) OVERRIDE;
    bool containsRelatedTarget(Event*);

    // Element
    virtual const AtomicString& shadowPseudoId() const OVERRIDE;

    MediaControllerInterface* m_mediaController;

    // Container for the media control elements.
    MediaControlPanelElement* m_panel;

    // Container for the text track cues.
    MediaControlTextTrackContainerElement* m_textDisplayContainer;

    // Media control elements.
    MediaControlPlayButtonElement* m_playButton;
    MediaControlCurrentTimeDisplayElement* m_currentTimeDisplay;
    MediaControlTimelineElement* m_timeline;
    MediaControlPanelMuteButtonElement* m_panelMuteButton;
    MediaControlPanelVolumeSliderElement* m_volumeSlider;
    MediaControlToggleClosedCaptionsButtonElement* m_toggleClosedCaptionsButton;
    MediaControlFullscreenButtonElement* m_fullScreenButton;
    MediaControlTimeRemainingDisplayElement* m_durationDisplay;
    MediaControlPanelEnclosureElement* m_enclosure;

    Timer<MediaControls> m_hideFullscreenControlsTimer;
    bool m_isFullscreen;
    bool m_isMouseOverControls;
};

DEFINE_NODE_TYPE_CASTS(MediaControls, isMediaControls());

}

#endif
