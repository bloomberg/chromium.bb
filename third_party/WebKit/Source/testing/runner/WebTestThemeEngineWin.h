/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

// This implements the WebThemeEngine API used by the Windows version of
// Chromium to render native form controls like checkboxes, radio buttons,
// and scroll bars.
// The normal implementation (native_theme) renders the controls using either
// the UXTheme theming engine present in XP, Vista, and Win 7, or the "classic"
// theme used if that theme is selected in the Desktop settings.
// Unfortunately, both of these themes render controls differently on the
// different versions of Windows.
//
// In order to ensure maximum consistency of baselines across the different
// Windows versions, we provide a simple implementation for DRT here
// instead. These controls are actually platform-independent (they're rendered
// using Skia) and could be used on Linux and the Mac as well, should we
// choose to do so at some point.
//

#ifndef WebTestThemeEngineWin_h
#define WebTestThemeEngineWin_h

#include "public/platform/WebNonCopyable.h"
#include "public/platform/win/WebThemeEngine.h"

namespace WebTestRunner {

class WebTestThemeEngineWin : public blink::WebThemeEngine, public blink::WebNonCopyable {
public:
    WebTestThemeEngineWin() { }
    virtual ~WebTestThemeEngineWin() { }

    // WebThemeEngine methods:
    virtual void paintButton(
        blink::WebCanvas*, int part, int state, int classicState,
        const blink::WebRect&);

    virtual void paintMenuList(
        blink::WebCanvas*, int part, int state, int classicState,
        const blink::WebRect&);

    virtual void paintScrollbarArrow(
        blink::WebCanvas*, int state, int classicState,
        const blink::WebRect&);

    virtual void paintScrollbarThumb(
        blink::WebCanvas*, int part, int state, int classicState,
        const blink::WebRect&);

    virtual void paintScrollbarTrack(
        blink::WebCanvas*, int part, int state, int classicState,
        const blink::WebRect&, const blink::WebRect& alignRect);

    virtual void paintSpinButton(
        blink::WebCanvas*, int part, int state, int classicState,
        const blink::WebRect&);

    virtual void paintTextField(
        blink::WebCanvas*, int part, int state, int classicState,
        const blink::WebRect&, blink::WebColor, bool fillContentArea,
        bool drawEdges);

    virtual void paintTrackbar(
        blink::WebCanvas*, int part, int state, int classicState,
        const blink::WebRect&);

    virtual void paintProgressBar(
        blink::WebCanvas*, const blink::WebRect& barRect,
        const blink::WebRect& valueRect,
        bool determinate, double time);

    virtual blink::WebSize getSize(int part);
};

}

#endif // WebTestThemeEngineWin_h
