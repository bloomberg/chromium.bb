// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediacapturefromelement/HTMLCanvasElementCapture.h"

#include <memory>
#include "core/dom/ExceptionCode.h"
#include "core/html/HTMLCanvasElement.h"
#include "modules/mediacapturefromelement/CanvasCaptureMediaStreamTrack.h"
#include "modules/mediastream/MediaStream.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCanvasCaptureHandler.h"
#include "public/platform/WebMediaStream.h"
#include "public/platform/WebMediaStreamTrack.h"

namespace {
const double kDefaultFrameRate = 60.0;
}  // anonymous namespace

namespace blink {

MediaStream* HTMLCanvasElementCapture::captureStream(
    ScriptState* script_state,
    HTMLCanvasElement& element,
    ExceptionState& exception_state) {
  return HTMLCanvasElementCapture::captureStream(script_state, element, false,
                                                 0, exception_state);
}

MediaStream* HTMLCanvasElementCapture::captureStream(
    ScriptState* script_state,
    HTMLCanvasElement& element,
    double frame_rate,
    ExceptionState& exception_state) {
  if (frame_rate < 0.0) {
    exception_state.ThrowDOMException(kNotSupportedError,
                                      "Given frame rate is not supported.");
    return nullptr;
  }

  return HTMLCanvasElementCapture::captureStream(script_state, element, true,
                                                 frame_rate, exception_state);
}

MediaStream* HTMLCanvasElementCapture::captureStream(
    ScriptState* script_state,
    HTMLCanvasElement& element,
    bool given_frame_rate,
    double frame_rate,
    ExceptionState& exception_state) {
  if (!element.OriginClean()) {
    exception_state.ThrowSecurityError("Canvas is not origin-clean.");
    return nullptr;
  }

  WebMediaStreamTrack track;
  const WebSize size(element.width(), element.height());
  std::unique_ptr<WebCanvasCaptureHandler> handler;
  if (given_frame_rate) {
    handler = Platform::Current()->CreateCanvasCaptureHandler(size, frame_rate,
                                                              &track);
  } else {
    handler = Platform::Current()->CreateCanvasCaptureHandler(
        size, kDefaultFrameRate, &track);
  }

  if (!handler) {
    exception_state.ThrowDOMException(
        kNotSupportedError, "No CanvasCapture handler can be created.");
    return nullptr;
  }

  ExecutionContext* context = ExecutionContext::From(script_state);
  DCHECK(context);
  CanvasCaptureMediaStreamTrack* canvas_track;
  if (given_frame_rate) {
    canvas_track = CanvasCaptureMediaStreamTrack::Create(
        track, &element, context, std::move(handler), frame_rate);
  } else {
    canvas_track = CanvasCaptureMediaStreamTrack::Create(
        track, &element, context, std::move(handler));
  }
  // We want to capture a frame in the beginning.
  canvas_track->requestFrame();

  MediaStreamTrackVector tracks;
  tracks.push_back(canvas_track);
  return MediaStream::Create(context, tracks);
}

}  // namespace blink
