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

#include "config.h"
#include "core/html/shadow/MediaControlElements.h"

#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/dom/DOMTokenList.h"
#include "core/events/EventNames.h"
#include "core/dom/FullscreenElementStack.h"
#include "core/events/MouseEvent.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html/shadow/MediaControls.h"
#include "core/html/track/TextTrack.h"
#include "core/html/track/TextTrackRegionList.h"
#include "core/page/EventHandler.h"
#include "core/page/Frame.h"
#include "core/page/Page.h"
#include "core/page/Settings.h"
#include "core/rendering/RenderMediaControlElements.h"
#include "core/rendering/RenderSlider.h"
#include "core/rendering/RenderTheme.h"
#include "core/rendering/RenderVideo.h"

namespace WebCore {

using namespace HTMLNames;

static const AtomicString& getMediaControlCurrentTimeDisplayElementShadowPseudoId();
static const AtomicString& getMediaControlTimeRemainingDisplayElementShadowPseudoId();

static const char* textTracksOffAttrValue = "-1"; // This must match HTMLMediaElement::textTracksOffIndex()

MediaControlPanelElement::MediaControlPanelElement(Document& document)
    : MediaControlDivElement(document, MediaControlsPanel)
    , m_canBeDragged(false)
    , m_isBeingDragged(false)
    , m_isDisplayed(false)
    , m_opaque(true)
    , m_transitionTimer(this, &MediaControlPanelElement::transitionTimerFired)
{
}

PassRefPtr<MediaControlPanelElement> MediaControlPanelElement::create(Document& document)
{
    return adoptRef(new MediaControlPanelElement(document));
}

const AtomicString& MediaControlPanelElement::part() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-panel", AtomicString::ConstructFromLiteral));
    return id;
}

void MediaControlPanelElement::startDrag(const LayoutPoint& eventLocation)
{
    if (!m_canBeDragged)
        return;

    if (m_isBeingDragged)
        return;

    RenderObject* renderer = this->renderer();
    if (!renderer || !renderer->isBox())
        return;

    Frame* frame = document().frame();
    if (!frame)
        return;

    m_lastDragEventLocation = eventLocation;

    frame->eventHandler()->setCapturingMouseEventsNode(this);

    m_isBeingDragged = true;
}

void MediaControlPanelElement::continueDrag(const LayoutPoint& eventLocation)
{
    if (!m_isBeingDragged)
        return;

    LayoutSize distanceDragged = eventLocation - m_lastDragEventLocation;
    m_cumulativeDragOffset.move(distanceDragged);
    m_lastDragEventLocation = eventLocation;
    setPosition(m_cumulativeDragOffset);
}

void MediaControlPanelElement::endDrag()
{
    if (!m_isBeingDragged)
        return;

    m_isBeingDragged = false;

    Frame* frame = document().frame();
    if (!frame)
        return;

    frame->eventHandler()->setCapturingMouseEventsNode(0);
}

void MediaControlPanelElement::startTimer()
{
    stopTimer();

    // The timer is required to set the property display:'none' on the panel,
    // such that captions are correctly displayed at the bottom of the video
    // at the end of the fadeout transition.
    double duration = RenderTheme::theme().mediaControlsFadeOutDuration();
    m_transitionTimer.startOneShot(duration);
}

void MediaControlPanelElement::stopTimer()
{
    if (m_transitionTimer.isActive())
        m_transitionTimer.stop();
}

void MediaControlPanelElement::transitionTimerFired(Timer<MediaControlPanelElement>*)
{
    if (!m_opaque)
        hide();

    stopTimer();
}

void MediaControlPanelElement::setPosition(const LayoutPoint& position)
{
    double left = position.x();
    double top = position.y();

    // Set the left and top to control the panel's position; this depends on it being absolute positioned.
    // Set the margin to zero since the position passed in will already include the effect of the margin.
    setInlineStyleProperty(CSSPropertyLeft, left, CSSPrimitiveValue::CSS_PX);
    setInlineStyleProperty(CSSPropertyTop, top, CSSPrimitiveValue::CSS_PX);
    setInlineStyleProperty(CSSPropertyMarginLeft, 0.0, CSSPrimitiveValue::CSS_PX);
    setInlineStyleProperty(CSSPropertyMarginTop, 0.0, CSSPrimitiveValue::CSS_PX);

    classList()->add("dragged", IGNORE_EXCEPTION);
}

void MediaControlPanelElement::resetPosition()
{
    removeInlineStyleProperty(CSSPropertyLeft);
    removeInlineStyleProperty(CSSPropertyTop);
    removeInlineStyleProperty(CSSPropertyMarginLeft);
    removeInlineStyleProperty(CSSPropertyMarginTop);

    classList()->remove("dragged", IGNORE_EXCEPTION);

    m_cumulativeDragOffset.setX(0);
    m_cumulativeDragOffset.setY(0);
}

void MediaControlPanelElement::makeOpaque()
{
    if (m_opaque)
        return;

    double duration = RenderTheme::theme().mediaControlsFadeInDuration();

    setInlineStyleProperty(CSSPropertyTransitionProperty, CSSPropertyOpacity);
    setInlineStyleProperty(CSSPropertyTransitionDuration, duration, CSSPrimitiveValue::CSS_S);
    setInlineStyleProperty(CSSPropertyOpacity, 1.0, CSSPrimitiveValue::CSS_NUMBER);

    m_opaque = true;

    if (m_isDisplayed)
        show();
}

void MediaControlPanelElement::makeTransparent()
{
    if (!m_opaque)
        return;

    double duration = RenderTheme::theme().mediaControlsFadeOutDuration();

    setInlineStyleProperty(CSSPropertyTransitionProperty, CSSPropertyOpacity);
    setInlineStyleProperty(CSSPropertyTransitionDuration, duration, CSSPrimitiveValue::CSS_S);
    setInlineStyleProperty(CSSPropertyOpacity, 0.0, CSSPrimitiveValue::CSS_NUMBER);

    m_opaque = false;
    startTimer();
}

void MediaControlPanelElement::defaultEventHandler(Event* event)
{
    MediaControlDivElement::defaultEventHandler(event);

    if (event->isMouseEvent()) {
        LayoutPoint location = toMouseEvent(event)->absoluteLocation();
        if (event->type() == eventNames().mousedownEvent && event->target() == this) {
            startDrag(location);
            event->setDefaultHandled();
        } else if (event->type() == eventNames().mousemoveEvent && m_isBeingDragged)
            continueDrag(location);
        else if (event->type() == eventNames().mouseupEvent && m_isBeingDragged) {
            continueDrag(location);
            endDrag();
            event->setDefaultHandled();
        }
    }
}

void MediaControlPanelElement::setCanBeDragged(bool canBeDragged)
{
    if (m_canBeDragged == canBeDragged)
        return;

    m_canBeDragged = canBeDragged;

    if (!canBeDragged)
        endDrag();
}

void MediaControlPanelElement::setIsDisplayed(bool isDisplayed)
{
    m_isDisplayed = isDisplayed;
}

// ----------------------------

MediaControlPanelEnclosureElement::MediaControlPanelEnclosureElement(Document& document)
    // Mapping onto same MediaControlElementType as panel element, since it has similar properties.
    : MediaControlDivElement(document, MediaControlsPanel)
{
}

PassRefPtr<MediaControlPanelEnclosureElement> MediaControlPanelEnclosureElement::create(Document& document)
{
    return adoptRef(new MediaControlPanelEnclosureElement(document));
}

const AtomicString& MediaControlPanelEnclosureElement::part() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-enclosure", AtomicString::ConstructFromLiteral));
    return id;
}

// ----------------------------

MediaControlOverlayEnclosureElement::MediaControlOverlayEnclosureElement(Document& document)
    // Mapping onto same MediaControlElementType as panel element, since it has similar properties.
    : MediaControlDivElement(document, MediaControlsPanel)
{
}

PassRefPtr<MediaControlOverlayEnclosureElement> MediaControlOverlayEnclosureElement::create(Document& document)
{
    return adoptRef(new MediaControlOverlayEnclosureElement(document));
}

const AtomicString& MediaControlOverlayEnclosureElement::part() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-overlay-enclosure", AtomicString::ConstructFromLiteral));
    return id;
}

// ----------------------------

MediaControlPanelMuteButtonElement::MediaControlPanelMuteButtonElement(Document& document, MediaControls* controls)
    : MediaControlMuteButtonElement(document, MediaMuteButton)
    , m_controls(controls)
{
}

PassRefPtr<MediaControlPanelMuteButtonElement> MediaControlPanelMuteButtonElement::create(Document& document, MediaControls* controls)
{
    ASSERT(controls);

    RefPtr<MediaControlPanelMuteButtonElement> button = adoptRef(new MediaControlPanelMuteButtonElement(document, controls));
    button->ensureUserAgentShadowRoot();
    button->setType("button");
    return button.release();
}

void MediaControlPanelMuteButtonElement::defaultEventHandler(Event* event)
{
    if (event->type() == eventNames().mouseoverEvent)
        m_controls->showVolumeSlider();

    MediaControlMuteButtonElement::defaultEventHandler(event);
}

const AtomicString& MediaControlPanelMuteButtonElement::part() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-mute-button", AtomicString::ConstructFromLiteral));
    return id;
}

// ----------------------------

MediaControlVolumeSliderMuteButtonElement::MediaControlVolumeSliderMuteButtonElement(Document& document)
    : MediaControlMuteButtonElement(document, MediaMuteButton)
{
}

PassRefPtr<MediaControlVolumeSliderMuteButtonElement> MediaControlVolumeSliderMuteButtonElement::create(Document& document)
{
    RefPtr<MediaControlVolumeSliderMuteButtonElement> button = adoptRef(new MediaControlVolumeSliderMuteButtonElement(document));
    button->ensureUserAgentShadowRoot();
    button->setType("button");
    return button.release();
}

const AtomicString& MediaControlVolumeSliderMuteButtonElement::part() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-volume-slider-mute-button", AtomicString::ConstructFromLiteral));
    return id;
}

// ----------------------------

MediaControlPlayButtonElement::MediaControlPlayButtonElement(Document& document)
    : MediaControlInputElement(document, MediaPlayButton)
{
}

PassRefPtr<MediaControlPlayButtonElement> MediaControlPlayButtonElement::create(Document& document)
{
    RefPtr<MediaControlPlayButtonElement> button = adoptRef(new MediaControlPlayButtonElement(document));
    button->ensureUserAgentShadowRoot();
    button->setType("button");
    return button.release();
}

void MediaControlPlayButtonElement::defaultEventHandler(Event* event)
{
    if (event->type() == eventNames().clickEvent) {
        if (mediaController()->canPlay())
            mediaController()->play();
        else
            mediaController()->pause();
        updateDisplayType();
        event->setDefaultHandled();
    }
    HTMLInputElement::defaultEventHandler(event);
}

void MediaControlPlayButtonElement::updateDisplayType()
{
    setDisplayType(mediaController()->canPlay() ? MediaPlayButton : MediaPauseButton);
}

const AtomicString& MediaControlPlayButtonElement::part() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-play-button", AtomicString::ConstructFromLiteral));
    return id;
}

// ----------------------------

MediaControlOverlayPlayButtonElement::MediaControlOverlayPlayButtonElement(Document& document)
    : MediaControlInputElement(document, MediaOverlayPlayButton)
{
}

PassRefPtr<MediaControlOverlayPlayButtonElement> MediaControlOverlayPlayButtonElement::create(Document& document)
{
    RefPtr<MediaControlOverlayPlayButtonElement> button = adoptRef(new MediaControlOverlayPlayButtonElement(document));
    button->ensureUserAgentShadowRoot();
    button->setType("button");
    return button.release();
}

void MediaControlOverlayPlayButtonElement::defaultEventHandler(Event* event)
{
    if (event->type() == eventNames().clickEvent && mediaController()->canPlay()) {
        mediaController()->play();
        updateDisplayType();
        event->setDefaultHandled();
    }
    HTMLInputElement::defaultEventHandler(event);
}

void MediaControlOverlayPlayButtonElement::updateDisplayType()
{
    if (mediaController()->canPlay()) {
        show();
    } else
        hide();
}

const AtomicString& MediaControlOverlayPlayButtonElement::part() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-overlay-play-button", AtomicString::ConstructFromLiteral));
    return id;
}


// ----------------------------

MediaControlToggleClosedCaptionsButtonElement::MediaControlToggleClosedCaptionsButtonElement(Document& document, MediaControls* controls)
    : MediaControlInputElement(document, MediaShowClosedCaptionsButton)
{
    UNUSED_PARAM(controls);
}

PassRefPtr<MediaControlToggleClosedCaptionsButtonElement> MediaControlToggleClosedCaptionsButtonElement::create(Document& document, MediaControls* controls)
{
    ASSERT(controls);

    RefPtr<MediaControlToggleClosedCaptionsButtonElement> button = adoptRef(new MediaControlToggleClosedCaptionsButtonElement(document, controls));
    button->ensureUserAgentShadowRoot();
    button->setType("button");
    button->hide();
    return button.release();
}

void MediaControlToggleClosedCaptionsButtonElement::updateDisplayType()
{
    bool captionsVisible = mediaController()->closedCaptionsVisible();
    setDisplayType(captionsVisible ? MediaHideClosedCaptionsButton : MediaShowClosedCaptionsButton);
    setChecked(captionsVisible);
}

void MediaControlToggleClosedCaptionsButtonElement::defaultEventHandler(Event* event)
{
    if (event->type() == eventNames().clickEvent) {
        mediaController()->setClosedCaptionsVisible(!mediaController()->closedCaptionsVisible());
        setChecked(mediaController()->closedCaptionsVisible());
        updateDisplayType();
        event->setDefaultHandled();
    }

    HTMLInputElement::defaultEventHandler(event);
}

const AtomicString& MediaControlToggleClosedCaptionsButtonElement::part() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-toggle-closed-captions-button", AtomicString::ConstructFromLiteral));
    return id;
}

// ----------------------------

MediaControlTimelineElement::MediaControlTimelineElement(Document& document, MediaControls* controls)
    : MediaControlInputElement(document, MediaSlider)
    , m_controls(controls)
{
}

PassRefPtr<MediaControlTimelineElement> MediaControlTimelineElement::create(Document& document, MediaControls* controls)
{
    ASSERT(controls);

    RefPtr<MediaControlTimelineElement> timeline = adoptRef(new MediaControlTimelineElement(document, controls));
    timeline->ensureUserAgentShadowRoot();
    timeline->setType("range");
    timeline->setAttribute(precisionAttr, "float");
    return timeline.release();
}

void MediaControlTimelineElement::defaultEventHandler(Event* event)
{
    // Left button is 0. Rejects mouse events not from left button.
    if (event->isMouseEvent() && toMouseEvent(event)->button())
        return;

    if (!attached())
        return;

    if (event->type() == eventNames().mousedownEvent)
        mediaController()->beginScrubbing();

    if (event->type() == eventNames().mouseupEvent)
        mediaController()->endScrubbing();

    MediaControlInputElement::defaultEventHandler(event);

    if (event->type() == eventNames().mouseoverEvent || event->type() == eventNames().mouseoutEvent || event->type() == eventNames().mousemoveEvent)
        return;

    double time = value().toDouble();
    if (event->type() == eventNames().inputEvent && time != mediaController()->currentTime())
        mediaController()->setCurrentTime(time, IGNORE_EXCEPTION);

    RenderSlider* slider = toRenderSlider(renderer());
    if (slider && slider->inDragMode())
        m_controls->updateCurrentTimeDisplay();
}

bool MediaControlTimelineElement::willRespondToMouseClickEvents()
{
    if (!attached())
        return false;

    return true;
}

void MediaControlTimelineElement::setPosition(double currentTime)
{
    setValue(String::number(currentTime));
}

void MediaControlTimelineElement::setDuration(double duration)
{
    setAttribute(maxAttr, String::number(std::isfinite(duration) ? duration : 0));
}


const AtomicString& MediaControlTimelineElement::part() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-timeline", AtomicString::ConstructFromLiteral));
    return id;
}

// ----------------------------

MediaControlPanelVolumeSliderElement::MediaControlPanelVolumeSliderElement(Document& document)
    : MediaControlVolumeSliderElement(document)
{
}

PassRefPtr<MediaControlPanelVolumeSliderElement> MediaControlPanelVolumeSliderElement::create(Document& document)
{
    RefPtr<MediaControlPanelVolumeSliderElement> slider = adoptRef(new MediaControlPanelVolumeSliderElement(document));
    slider->ensureUserAgentShadowRoot();
    slider->setType("range");
    slider->setAttribute(precisionAttr, "float");
    slider->setAttribute(maxAttr, "1");
    return slider.release();
}

const AtomicString& MediaControlPanelVolumeSliderElement::part() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-volume-slider", AtomicString::ConstructFromLiteral));
    return id;
}

// ----------------------------

MediaControlFullscreenButtonElement::MediaControlFullscreenButtonElement(Document& document)
    : MediaControlInputElement(document, MediaEnterFullscreenButton)
{
}

PassRefPtr<MediaControlFullscreenButtonElement> MediaControlFullscreenButtonElement::create(Document& document)
{
    RefPtr<MediaControlFullscreenButtonElement> button = adoptRef(new MediaControlFullscreenButtonElement(document));
    button->ensureUserAgentShadowRoot();
    button->setType("button");
    button->hide();
    return button.release();
}

void MediaControlFullscreenButtonElement::defaultEventHandler(Event* event)
{
    if (event->type() == eventNames().clickEvent) {
        // Only use the new full screen API if the fullScreenEnabled setting has
        // been explicitly enabled. Otherwise, use the old fullscreen API. This
        // allows apps which embed a WebView to retain the existing full screen
        // video implementation without requiring them to implement their own full
        // screen behavior.
        if (document().settings() && document().settings()->fullScreenEnabled()) {
            if (FullscreenElementStack::isActiveFullScreenElement(toParentMediaElement(this)))
                FullscreenElementStack::from(&document())->webkitCancelFullScreen();
            else
                FullscreenElementStack::from(&document())->requestFullScreenForElement(toParentMediaElement(this), 0, FullscreenElementStack::ExemptIFrameAllowFullScreenRequirement);
        } else
            mediaController()->enterFullscreen();
        event->setDefaultHandled();
    }
    HTMLInputElement::defaultEventHandler(event);
}

const AtomicString& MediaControlFullscreenButtonElement::part() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-fullscreen-button", AtomicString::ConstructFromLiteral));
    return id;
}

void MediaControlFullscreenButtonElement::setIsFullscreen(bool isFullscreen)
{
    setDisplayType(isFullscreen ? MediaExitFullscreenButton : MediaEnterFullscreenButton);
}

// ----------------------------

MediaControlTimeRemainingDisplayElement::MediaControlTimeRemainingDisplayElement(Document& document)
    : MediaControlTimeDisplayElement(document, MediaTimeRemainingDisplay)
{
}

PassRefPtr<MediaControlTimeRemainingDisplayElement> MediaControlTimeRemainingDisplayElement::create(Document& document)
{
    return adoptRef(new MediaControlTimeRemainingDisplayElement(document));
}

static const AtomicString& getMediaControlTimeRemainingDisplayElementShadowPseudoId()
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-time-remaining-display", AtomicString::ConstructFromLiteral));
    return id;
}

const AtomicString& MediaControlTimeRemainingDisplayElement::part() const
{
    return getMediaControlTimeRemainingDisplayElementShadowPseudoId();
}

// ----------------------------

MediaControlCurrentTimeDisplayElement::MediaControlCurrentTimeDisplayElement(Document& document)
    : MediaControlTimeDisplayElement(document, MediaCurrentTimeDisplay)
{
}

PassRefPtr<MediaControlCurrentTimeDisplayElement> MediaControlCurrentTimeDisplayElement::create(Document& document)
{
    return adoptRef(new MediaControlCurrentTimeDisplayElement(document));
}

static const AtomicString& getMediaControlCurrentTimeDisplayElementShadowPseudoId()
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-current-time-display", AtomicString::ConstructFromLiteral));
    return id;
}

const AtomicString& MediaControlCurrentTimeDisplayElement::part() const
{
    return getMediaControlCurrentTimeDisplayElementShadowPseudoId();
}

// ----------------------------

MediaControlTextTrackContainerElement::MediaControlTextTrackContainerElement(Document& document)
    : MediaControlDivElement(document, MediaTextTrackDisplayContainer)
    , m_fontSize(0)
{
}

PassRefPtr<MediaControlTextTrackContainerElement> MediaControlTextTrackContainerElement::create(Document& document)
{
    RefPtr<MediaControlTextTrackContainerElement> element = adoptRef(new MediaControlTextTrackContainerElement(document));
    element->hide();
    return element.release();
}

RenderObject* MediaControlTextTrackContainerElement::createRenderer(RenderStyle*)
{
    return new RenderTextTrackContainerElement(this);
}

const AtomicString& MediaControlTextTrackContainerElement::textTrackContainerElementShadowPseudoId()
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-text-track-container", AtomicString::ConstructFromLiteral));
    return id;
}

const AtomicString& MediaControlTextTrackContainerElement::part() const
{
    return textTrackContainerElementShadowPseudoId();
}

void MediaControlTextTrackContainerElement::updateDisplay()
{
    if (!mediaController()->closedCaptionsVisible()) {
        removeChildren();
        return;
    }

    HTMLMediaElement* mediaElement = toParentMediaElement(this);
    // 1. If the media element is an audio element, or is another playback
    // mechanism with no rendering area, abort these steps. There is nothing to
    // render.
    if (!mediaElement || !mediaElement->isVideo())
        return;

    // 2. Let video be the media element or other playback mechanism.
    HTMLVideoElement* video = toHTMLVideoElement(mediaElement);

    // 3. Let output be an empty list of absolutely positioned CSS block boxes.
    Vector<RefPtr<HTMLDivElement> > output;

    // 4. If the user agent is exposing a user interface for video, add to
    // output one or more completely transparent positioned CSS block boxes that
    // cover the same region as the user interface.

    // 5. If the last time these rules were run, the user agent was not exposing
    // a user interface for video, but now it is, let reset be true. Otherwise,
    // let reset be false.

    // There is nothing to be done explicitly for 4th and 5th steps, as
    // everything is handled through CSS. The caption box is on top of the
    // controls box, in a container set with the -webkit-box display property.

    // 6. Let tracks be the subset of video's list of text tracks that have as
    // their rules for updating the text track rendering these rules for
    // updating the display of WebVTT text tracks, and whose text track mode is
    // showing or showing by default.
    // 7. Let cues be an empty list of text track cues.
    // 8. For each track track in tracks, append to cues all the cues from
    // track's list of cues that have their text track cue active flag set.
    CueList activeCues = video->currentlyActiveCues();

    // 9. If reset is false, then, for each text track cue cue in cues: if cue's
    // text track cue display state has a set of CSS boxes, then add those boxes
    // to output, and remove cue from cues.

    // There is nothing explicitly to be done here, as all the caching occurs
    // within the TextTrackCue instance itself. If parameters of the cue change,
    // the display tree is cleared.

    // 10. For each text track cue cue in cues that has not yet had
    // corresponding CSS boxes added to output, in text track cue order, run the
    // following substeps:
    for (size_t i = 0; i < activeCues.size(); ++i) {
        TextTrackCue* cue = activeCues[i].data();

        ASSERT(cue->isActive());
        if (!cue->track() || !cue->track()->isRendered() || !cue->isActive())
            continue;

        RefPtr<TextTrackCueBox> displayBox = cue->getDisplayTree(m_videoDisplaySize.size());

#if ENABLE(WEBVTT_REGIONS)
        String regionId = cue->regionId();
        TextTrackRegion* region = cue->track()->regions()->getRegionById(regionId);
        if (!region) {
            // If cue has an empty text track cue region identifier or there is no
            // WebVTT region whose region identifier is identical to cue's text
            // track cue region identifier, run the following substeps:
#endif
            if (displayBox->hasChildNodes() && !contains(displayBox.get())) {
                // Note: the display tree of a cue is removed when the active flag of the cue is unset.
                appendChild(displayBox);
            }
#if ENABLE(WEBVTT_REGIONS)
        } else {
            // Let region be the WebVTT region whose region identifier
            // matches the text track cue region identifier of cue.
            RefPtr<HTMLDivElement> regionNode = region->getDisplayTree();

            // Append the region to the viewport, if it was not already.
            if (!contains(regionNode.get()))
                appendChild(region->getDisplayTree());

            region->appendTextTrackCueBox(displayBox);
        }
#endif
    }

    // 11. Return output.
    if (hasChildNodes())
        show();
    else
        hide();
}

void MediaControlTextTrackContainerElement::updateSizes(bool forceUpdate)
{
    HTMLMediaElement* mediaElement = toParentMediaElement(this);
    if (!mediaElement)
        return;

    if (!document().page())
        return;

    IntRect videoBox;

    if (!mediaElement->renderer() || !mediaElement->renderer()->isVideo())
        return;
    videoBox = toRenderVideo(mediaElement->renderer())->videoBox();

    if (!forceUpdate && m_videoDisplaySize == videoBox)
        return;
    m_videoDisplaySize = videoBox;

    float smallestDimension = std::min(m_videoDisplaySize.size().height(), m_videoDisplaySize.size().width());

    float fontSize = smallestDimension * 0.05f;
    if (fontSize != m_fontSize) {
        m_fontSize = fontSize;
        setInlineStyleProperty(CSSPropertyFontSize, fontSize, CSSPrimitiveValue::CSS_PX);
    }

    CueList activeCues = mediaElement->currentlyActiveCues();
    for (size_t i = 0; i < activeCues.size(); ++i) {
        TextTrackCue* cue = activeCues[i].data();
        cue->videoSizeDidChange(m_videoDisplaySize.size());
    }
}

// ----------------------------

} // namespace WebCore
