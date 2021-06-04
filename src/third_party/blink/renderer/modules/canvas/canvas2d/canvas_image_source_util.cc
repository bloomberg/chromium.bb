// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_image_source_util.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_union_cssimagevalue_htmlcanvaselement_htmlimageelement_htmlvideoelement_imagebitmap_offscreencanvas_svgimageelement_videoframe.h"
#include "third_party/blink/renderer/core/css/cssom/css_url_image_value.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/core/svg/svg_image_element.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"

namespace blink {

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
CanvasImageSource* ToCanvasImageSource(const V8CanvasImageSource* value,
                                       ExceptionState& exception_state) {
  DCHECK(value);

  switch (value->GetContentType()) {
    case V8CanvasImageSource::ContentType::kCSSImageValue:
      return value->GetAsCSSImageValue();
    case V8CanvasImageSource::ContentType::kHTMLCanvasElement: {
      if (value->GetAsHTMLCanvasElement()->Size().IsEmpty()) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kInvalidStateError,
            "The image argument is a canvas element with a width "
            "or height of 0.");
        return nullptr;
      }
      return value->GetAsHTMLCanvasElement();
    }
    case V8CanvasImageSource::ContentType::kHTMLImageElement:
      return value->GetAsHTMLImageElement();
    case V8CanvasImageSource::ContentType::kHTMLVideoElement: {
      HTMLVideoElement* video = value->GetAsHTMLVideoElement();
      video->VideoWillBeDrawnToCanvas();
      return video;
    }
    case V8CanvasImageSource::ContentType::kImageBitmap:
      if (value->GetAsImageBitmap()->IsNeutered()) {
        exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                          "The image source is detached");
        return nullptr;
      }
      return value->GetAsImageBitmap();
    case V8CanvasImageSource::ContentType::kOffscreenCanvas:
      if (value->GetAsOffscreenCanvas()->IsNeutered()) {
        exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                          "The image source is detached");
        return nullptr;
      }
      if (value->GetAsOffscreenCanvas()->Size().IsEmpty()) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kInvalidStateError,
            "The image argument is an OffscreenCanvas element "
            "with a width or height of 0.");
        return nullptr;
      }
      return value->GetAsOffscreenCanvas();
    case V8CanvasImageSource::ContentType::kSVGImageElement:
      return value->GetAsSVGImageElement();
    case V8CanvasImageSource::ContentType::kVideoFrame: {
      VideoFrame* video_frame = value->GetAsVideoFrame();
      if (!video_frame->frame()) {
        exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                          "The VideoFrame has been closed");
        return nullptr;
      }
      return video_frame;
    }
  }

  NOTREACHED();
  return nullptr;
}
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
CanvasImageSource* ToCanvasImageSource(const CanvasImageSourceUnion& value,
                                       ExceptionState& exception_state) {
  if (value.IsCSSImageValue())
    return value.GetAsCSSImageValue();
  if (value.IsHTMLImageElement())
    return value.GetAsHTMLImageElement();
  if (value.IsHTMLVideoElement()) {
    HTMLVideoElement* video = value.GetAsHTMLVideoElement();
    video->VideoWillBeDrawnToCanvas();
    return video;
  }
  if (value.IsSVGImageElement())
    return value.GetAsSVGImageElement();
  if (value.IsHTMLCanvasElement()) {
    if (static_cast<HTMLCanvasElement*>(value.GetAsHTMLCanvasElement())
            ->Size()
            .IsEmpty()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "The image argument is a canvas element with a width "
          "or height of 0.");
      return nullptr;
    }
    return value.GetAsHTMLCanvasElement();
  }
  if (value.IsImageBitmap()) {
    if (static_cast<ImageBitmap*>(value.GetAsImageBitmap())->IsNeutered()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "The image source is detached");
      return nullptr;
    }
    return value.GetAsImageBitmap();
  }
  if (value.IsOffscreenCanvas()) {
    if (static_cast<OffscreenCanvas*>(value.GetAsOffscreenCanvas())
            ->IsNeutered()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "The image source is detached");
      return nullptr;
    }
    if (static_cast<OffscreenCanvas*>(value.GetAsOffscreenCanvas())
            ->Size()
            .IsEmpty()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "The image argument is an OffscreenCanvas element "
          "with a width or height of 0.");
      return nullptr;
    }
    return value.GetAsOffscreenCanvas();
  }
  if (value.IsVideoFrame()) {
    auto* video_frame = static_cast<VideoFrame*>(value.GetAsVideoFrame());
    if (!video_frame->frame()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "The VideoFrame has been closed");
      return nullptr;
    }
    return video_frame;
  }
  NOTREACHED();
  return nullptr;
}
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

}  // namespace blink
