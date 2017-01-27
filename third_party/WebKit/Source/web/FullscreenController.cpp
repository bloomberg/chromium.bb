/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "web/FullscreenController.h"

#include "core/dom/Document.h"
#include "core/dom/Fullscreen.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/PageScaleConstraintsSet.h"
#include "core/html/HTMLVideoElement.h"
#include "core/layout/LayoutFullScreen.h"
#include "public/platform/WebLayerTreeView.h"
#include "public/web/WebFrameClient.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebViewImpl.h"

namespace blink {

namespace {

WebFrameClient& webFrameClient(LocalFrame& frame) {
  WebLocalFrameImpl* webFrame = WebLocalFrameImpl::fromFrame(frame);
  DCHECK(webFrame);
  DCHECK(webFrame->client());
  return *webFrame->client();
}

}  // anonymous namespace

std::unique_ptr<FullscreenController> FullscreenController::create(
    WebViewImpl* webViewImpl) {
  return WTF::wrapUnique(new FullscreenController(webViewImpl));
}

FullscreenController::FullscreenController(WebViewImpl* webViewImpl)
    : m_webViewImpl(webViewImpl) {}

void FullscreenController::didEnterFullscreen() {
  // |Browser::EnterFullscreenModeForTab()| can enter fullscreen without going
  // through |Fullscreen::requestFullscreen()|, in which case there will be no
  // fullscreen element. Do nothing.
  if (m_state != State::EnteringFullscreen)
    return;

  updatePageScaleConstraints(false);
  m_webViewImpl->setPageScaleFactor(1.0f);
  if (m_webViewImpl->mainFrame()->isWebLocalFrame())
    m_webViewImpl->mainFrame()->setScrollOffset(WebSize());
  m_webViewImpl->setVisualViewportOffset(FloatPoint());

  m_state = State::Fullscreen;

  // Notify all local frames that we have entered fullscreen.
  for (Frame* frame = m_webViewImpl->page()->mainFrame(); frame;
       frame = frame->tree().traverseNext()) {
    if (!frame->isLocalFrame())
      continue;
    if (Document* document = toLocalFrame(frame)->document()) {
      if (Fullscreen* fullscreen = Fullscreen::fromIfExists(*document))
        fullscreen->didEnterFullscreen();
    }
  }
}

void FullscreenController::didExitFullscreen() {
  // The browser process can exit fullscreen at any time, e.g. if the user
  // presses Esc. After |Browser::EnterFullscreenModeForTab()|,
  // |Browser::ExitFullscreenModeForTab()| will make it seem like we exit when
  // not even in fullscreen. Do nothing.
  if (m_state == State::Initial)
    return;

  updatePageScaleConstraints(true);

  // Set |m_state| so that any |exitFullscreen()| calls from within
  // |Fullscreen::didExitFullscreen()| do not call
  // |WebFrameClient::exitFullscreen()| again.
  // TODO(foolip): Remove this when state changes and events are synchronized
  // with animation frames. https://crbug.com/402376
  m_state = State::ExitingFullscreen;

  // Notify all local frames that we have exited fullscreen.
  // TODO(foolip): This should only need to notify the topmost local roots. That
  // doesn't currently work because |Fullscreen::m_currentFullScreenElement|
  // isn't set for the topmost document when an iframe goes fullscreen, but can
  // be done once |m_currentFullScreenElement| is gone and all state is in the
  // fullscreen element stack. https://crbug.com/402421
  for (Frame* frame = m_webViewImpl->page()->mainFrame(); frame;
       frame = frame->tree().traverseNext()) {
    if (!frame->isLocalFrame())
      continue;
    if (Document* document = toLocalFrame(frame)->document()) {
      if (Fullscreen* fullscreen = Fullscreen::fromIfExists(*document))
        fullscreen->didExitFullscreen();
    }
  }

  // We need to wait until style and layout are updated in order to properly
  // restore scroll offsets since content may not be overflowing in the same way
  // until they are.
  m_state = State::NeedsScrollAndScaleRestore;
}

void FullscreenController::enterFullscreen(LocalFrame& frame) {
  // If already fullscreen or exiting fullscreen, synchronously call
  // |didEnterFullscreen()|. When exiting, the coming |didExitFullscren()| call
  // will again notify all frames.
  if (m_state == State::Fullscreen || m_state == State::ExitingFullscreen) {
    State oldState = m_state;
    m_state = State::EnteringFullscreen;
    didEnterFullscreen();
    m_state = oldState;
    return;
  }

  // We need to store these values here rather than in |didEnterFullscreen()|
  // since by the time the latter is called, a Resize has already occured,
  // clamping the scroll offset. Don't save values if we're still waiting to
  // restore a previous set. This can happen if we exit and quickly reenter
  // fullscreen without performing a layout.
  if (m_state == State::Initial) {
    m_initialPageScaleFactor = m_webViewImpl->pageScaleFactor();
    m_initialScrollOffset = m_webViewImpl->mainFrame()->isWebLocalFrame()
                                ? m_webViewImpl->mainFrame()->getScrollOffset()
                                : WebSize();
    m_initialVisualViewportOffset = m_webViewImpl->visualViewportOffset();
  }

  // If already entering fullscreen, just wait.
  if (m_state == State::EnteringFullscreen)
    return;

  DCHECK(m_state == State::Initial ||
         m_state == State::NeedsScrollAndScaleRestore);
  webFrameClient(frame).enterFullscreen();

  m_state = State::EnteringFullscreen;
}

void FullscreenController::exitFullscreen(LocalFrame& frame) {
  // If not in fullscreen, ignore any attempt to exit. In particular, when
  // entering fullscreen, allow the transition into fullscreen to complete. Note
  // that the browser process is ultimately in control and can still exit
  // fullscreen at any time.
  if (m_state != State::Fullscreen)
    return;

  webFrameClient(frame).exitFullscreen();

  m_state = State::ExitingFullscreen;
}

void FullscreenController::fullscreenElementChanged(Element* fromElement,
                                                    Element* toElement) {
  DCHECK_NE(fromElement, toElement);

  if (toElement) {
    DCHECK(Fullscreen::isCurrentFullScreenElement(*toElement));

    if (isHTMLVideoElement(*toElement)) {
      HTMLVideoElement& videoElement = toHTMLVideoElement(*toElement);
      videoElement.didEnterFullscreen();

      // If the video uses overlay fullscreen mode, make the background
      // transparent.
      if (videoElement.usesOverlayFullscreenVideo() &&
          m_webViewImpl->layerTreeView()) {
        m_webViewImpl->layerTreeView()->setHasTransparentBackground(true);
      }
    }
  }

  if (fromElement) {
    DCHECK(!Fullscreen::isCurrentFullScreenElement(*fromElement));

    if (isHTMLVideoElement(*fromElement)) {
      // If the video used overlay fullscreen mode, restore the transparency.
      if (m_webViewImpl->layerTreeView()) {
        m_webViewImpl->layerTreeView()->setHasTransparentBackground(
            m_webViewImpl->isTransparent());
      }

      HTMLVideoElement& videoElement = toHTMLVideoElement(*fromElement);
      videoElement.didExitFullscreen();
    }
  }
}

void FullscreenController::updateSize() {
  DCHECK(m_webViewImpl->page());

  if (m_state != State::Fullscreen && m_state != State::ExitingFullscreen)
    return;

  updatePageScaleConstraints(false);

  // Traverse all local frames and notify the LayoutFullScreen object, if any.
  for (Frame* frame = m_webViewImpl->page()->mainFrame(); frame;
       frame = frame->tree().traverseNext()) {
    if (!frame->isLocalFrame())
      continue;
    if (Document* document = toLocalFrame(frame)->document()) {
      if (Fullscreen* fullscreen = Fullscreen::fromIfExists(*document)) {
        if (LayoutFullScreen* layoutObject =
                fullscreen->fullScreenLayoutObject())
          layoutObject->updateStyle();
      }
    }
  }
}

void FullscreenController::didUpdateLayout() {
  if (m_state != State::NeedsScrollAndScaleRestore)
    return;

  m_webViewImpl->setPageScaleFactor(m_initialPageScaleFactor);
  if (m_webViewImpl->mainFrame()->isWebLocalFrame())
    m_webViewImpl->mainFrame()->setScrollOffset(WebSize(m_initialScrollOffset));
  m_webViewImpl->setVisualViewportOffset(m_initialVisualViewportOffset);

  m_state = State::Initial;
}

void FullscreenController::updatePageScaleConstraints(bool removeConstraints) {
  PageScaleConstraints fullscreenConstraints;
  if (!removeConstraints) {
    fullscreenConstraints = PageScaleConstraints(1.0, 1.0, 1.0);
    fullscreenConstraints.layoutSize = FloatSize(m_webViewImpl->size());
  }
  m_webViewImpl->pageScaleConstraintsSet().setFullscreenConstraints(
      fullscreenConstraints);
  m_webViewImpl->pageScaleConstraintsSet().computeFinalConstraints();

  // Although we called |computedFinalConstraints()| above, the "final"
  // constraints are not actually final. They are still subject to scale factor
  // clamping by contents size. Normally they should be dirtied due to contents
  // size mutation after layout, however the contents size is not guaranteed to
  // mutate, and the scale factor may remain unclamped. Just fire the event
  // again to ensure the final constraints pick up the latest contents size.
  m_webViewImpl->didChangeContentsSize();
  if (m_webViewImpl->mainFrameImpl() &&
      m_webViewImpl->mainFrameImpl()->frameView())
    m_webViewImpl->mainFrameImpl()->frameView()->setNeedsLayout();

  m_webViewImpl->updateMainFrameLayoutSize();
}

}  // namespace blink
