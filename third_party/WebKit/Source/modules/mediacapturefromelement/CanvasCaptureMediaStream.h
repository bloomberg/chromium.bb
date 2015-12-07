// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CanvasCaptureMediaStream_h
#define CanvasCaptureMediaStream_h

#include "core/html/canvas/CanvasDrawListener.h"
#include "modules/mediastream/MediaStream.h"
#include "platform/heap/Handle.h"

namespace blink {

class HTMLCanvasElement;
class WebCanvasCaptureHandler;

class CanvasCaptureMediaStream final : public MediaStream {
    DEFINE_WRAPPERTYPEINFO();

public:
    static CanvasCaptureMediaStream* create(MediaStreamDescriptor*, PassRefPtrWillBeRawPtr<HTMLCanvasElement>);
    static CanvasCaptureMediaStream* create(MediaStreamDescriptor*, PassRefPtrWillBeRawPtr<HTMLCanvasElement>, const PassOwnPtr<WebCanvasCaptureHandler>);
    DECLARE_VIRTUAL_TRACE();
    HTMLCanvasElement* canvas() const;
    void requestFrame();

private:
    CanvasCaptureMediaStream(MediaStreamDescriptor*, PassRefPtrWillBeRawPtr<HTMLCanvasElement>);
    void initialize(const PassOwnPtr<WebCanvasCaptureHandler>);

    RefPtrWillBeMember<HTMLCanvasElement> m_canvasElement;
    Member<CanvasDrawListener> m_drawListener;
};

} // namespace blink

#endif
