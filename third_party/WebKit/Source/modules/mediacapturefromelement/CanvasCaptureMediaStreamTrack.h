// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CanvasCaptureMediaStreamTrack_h
#define CanvasCaptureMediaStreamTrack_h

#include "core/html/canvas/CanvasDrawListener.h"
#include "modules/mediastream/MediaStreamTrack.h"
#include "platform/heap/Handle.h"

namespace blink {

class HTMLCanvasElement;
class WebCanvasCaptureHandler;

class CanvasCaptureMediaStreamTrack final : public MediaStreamTrack {
    DEFINE_WRAPPERTYPEINFO();
public:
    static CanvasCaptureMediaStreamTrack* create(MediaStreamComponent*, HTMLCanvasElement*, PassOwnPtr<WebCanvasCaptureHandler>);
    static CanvasCaptureMediaStreamTrack* create(MediaStreamComponent*, HTMLCanvasElement*, PassOwnPtr<WebCanvasCaptureHandler>, double frameRate);

    HTMLCanvasElement* canvas() const;
    void requestFrame();

    CanvasCaptureMediaStreamTrack* clone(ExecutionContext*) override;

    DECLARE_VIRTUAL_TRACE();

private:
    CanvasCaptureMediaStreamTrack(const CanvasCaptureMediaStreamTrack&, MediaStreamComponent*);
    CanvasCaptureMediaStreamTrack(MediaStreamComponent*, HTMLCanvasElement*, PassOwnPtr<WebCanvasCaptureHandler>);
    CanvasCaptureMediaStreamTrack(MediaStreamComponent*, HTMLCanvasElement*, PassOwnPtr<WebCanvasCaptureHandler>, double frameRate);

    Member<HTMLCanvasElement> m_canvasElement;
    Member<CanvasDrawListener> m_drawListener;
};

} // namespace blink

#endif
