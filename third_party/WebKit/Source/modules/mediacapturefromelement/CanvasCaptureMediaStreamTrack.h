// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CanvasCaptureMediaStreamTrack_h
#define CanvasCaptureMediaStreamTrack_h

#include "core/html/canvas/CanvasDrawListener.h"
#include "modules/mediastream/MediaStreamTrack.h"
#include "platform/heap/Handle.h"
#include <memory>

namespace blink {

class ExecutionContext;
class HTMLCanvasElement;
class WebCanvasCaptureHandler;

class CanvasCaptureMediaStreamTrack final : public MediaStreamTrack {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CanvasCaptureMediaStreamTrack* Create(
      MediaStreamComponent*,
      HTMLCanvasElement*,
      ExecutionContext*,
      std::unique_ptr<WebCanvasCaptureHandler>);
  static CanvasCaptureMediaStreamTrack* Create(
      MediaStreamComponent*,
      HTMLCanvasElement*,
      ExecutionContext*,
      std::unique_ptr<WebCanvasCaptureHandler>,
      double frame_rate);

  HTMLCanvasElement* canvas() const;
  void requestFrame();

  CanvasCaptureMediaStreamTrack* clone(ScriptState*) override;

  void Trace(blink::Visitor*) override;

 private:
  CanvasCaptureMediaStreamTrack(const CanvasCaptureMediaStreamTrack&,
                                MediaStreamComponent*);
  CanvasCaptureMediaStreamTrack(MediaStreamComponent*,
                                HTMLCanvasElement*,
                                ExecutionContext*,
                                std::unique_ptr<WebCanvasCaptureHandler>);
  CanvasCaptureMediaStreamTrack(MediaStreamComponent*,
                                HTMLCanvasElement*,
                                ExecutionContext*,
                                std::unique_ptr<WebCanvasCaptureHandler>,
                                double frame_rate);

  Member<HTMLCanvasElement> canvas_element_;
  Member<CanvasDrawListener> draw_listener_;
};

}  // namespace blink

#endif
