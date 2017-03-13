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

#ifndef FullscreenController_h
#define FullscreenController_h

#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/IntSize.h"
#include <memory>

namespace blink {

class Element;
class LocalFrame;
class WebViewImpl;

class FullscreenController {
 public:
  static std::unique_ptr<FullscreenController> create(WebViewImpl*);

  // Called by Fullscreen (via ChromeClient) to request entering or exiting
  // fullscreen.
  void enterFullscreen(LocalFrame&);
  void exitFullscreen(LocalFrame&);

  // Called by content::RenderWidget (via WebWidget) to notify that we've
  // entered or exited fullscreen. This can be because we requested it, or it
  // can be initiated by the browser directly.
  void didEnterFullscreen();
  void didExitFullscreen();

  // Called by Fullscreen (via ChromeClient) to notify that the fullscreen
  // element has changed.
  void fullscreenElementChanged(Element*, Element*);

  bool isFullscreenOrTransitioning() const { return m_state != State::Initial; }

  void updateSize();

  void didUpdateLayout();

 protected:
  explicit FullscreenController(WebViewImpl*);

 private:
  void updatePageScaleConstraints(bool removeConstraints);

  WebViewImpl* m_webViewImpl;

  // State is used to avoid unnecessary enter/exit requests, and to restore the
  // m_initial* after the first layout upon exiting fullscreen. Typically, the
  // state goes through every state from Initial to NeedsScrollAndScaleRestore
  // and then back to Initial, but the are two exceptions:
  //  1. didExitFullscreen() can transition from any non-Initial state to
  //     NeedsScrollAndScaleRestore, in case of a browser-intiated exit.
  //  2. enterFullscreen() can transition from NeedsScrollAndScaleRestore to
  //     EnteringFullscreen, in case of a quick exit+enter.
  enum class State {
    Initial,
    EnteringFullscreen,
    Fullscreen,
    ExitingFullscreen,
    NeedsScrollAndScaleRestore
  };
  State m_state = State::Initial;

  float m_initialPageScaleFactor = 0.0f;
  IntSize m_initialScrollOffset;
  FloatPoint m_initialVisualViewportOffset;
};

}  // namespace blink

#endif
