/*
 * Copyright (C) 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef MediaControlElementTypes_h
#define MediaControlElementTypes_h

#include "core/html/HTMLDivElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/MediaControllerInterface.h"
#include "core/rendering/RenderBlock.h"

namespace WebCore {

enum MediaControlElementType {
    MediaEnterFullscreenButton = 0,
    MediaMuteButton,
    MediaPlayButton,
    MediaSlider,
    MediaSliderThumb,
    MediaShowClosedCaptionsButton,
    MediaHideClosedCaptionsButton,
    MediaUnMuteButton,
    MediaPauseButton,
    MediaTimelineContainer,
    MediaCurrentTimeDisplay,
    MediaTimeRemainingDisplay,
    MediaStatusDisplay,
    MediaControlsPanel,
    MediaVolumeSliderContainer,
    MediaVolumeSlider,
    MediaVolumeSliderThumb,
    MediaFullScreenVolumeSlider,
    MediaFullScreenVolumeSliderThumb,
    MediaTextTrackDisplayContainer,
    MediaTextTrackDisplay,
    MediaExitFullscreenButton,
    MediaOverlayPlayButton,
};

HTMLMediaElement* toParentMediaElement(Node*);
inline HTMLMediaElement* toParentMediaElement(RenderObject* renderer) { return toParentMediaElement(renderer->node()); }

MediaControlElementType mediaControlElementType(Node*);

// ----------------------------

class MediaControlElement {
public:
    void hide();
    void show();

    MediaControlElementType displayType() { return m_displayType; }

    void setMediaController(MediaControllerInterface* controller) { m_mediaController = controller; }
    MediaControllerInterface* mediaController() const { return m_mediaController; }

protected:
    explicit MediaControlElement(MediaControlElementType, HTMLElement*);
    ~MediaControlElement() { }

    void setDisplayType(MediaControlElementType);

private:
    MediaControllerInterface* m_mediaController;
    MediaControlElementType m_displayType;
    HTMLElement* m_element;
};

// ----------------------------

class MediaControlDivElement : public HTMLDivElement, public MediaControlElement {
protected:
    virtual bool isMediaControlElement() const OVERRIDE FINAL { return true; }
    explicit MediaControlDivElement(Document&, MediaControlElementType);
};

// ----------------------------

class MediaControlInputElement : public HTMLInputElement, public MediaControlElement {
protected:
    virtual bool isMediaControlElement() const OVERRIDE FINAL { return true; }
    explicit MediaControlInputElement(Document&, MediaControlElementType);

private:
    virtual void updateDisplayType() { }
    virtual bool isMouseFocusable() const OVERRIDE;
};

// ----------------------------

class MediaControlTimeDisplayElement : public MediaControlDivElement {
public:
    void setCurrentValue(double);
    double currentValue() const { return m_currentValue; }

protected:
    explicit MediaControlTimeDisplayElement(Document&, MediaControlElementType);

private:
    double m_currentValue;
};

} // namespace WebCore

#endif // MediaControlElementTypes_h
