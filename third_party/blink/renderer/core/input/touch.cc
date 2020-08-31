/*
 * Copyright 2008, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/input/touch.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_touch_init.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/geometry/float_point.h"

namespace blink {

namespace {

FloatPoint ContentsOffset(LocalFrame* frame) {
  if (!frame)
    return FloatPoint();
  LocalFrameView* frame_view = frame->View();
  if (!frame_view)
    return FloatPoint();
  float scale = 1.0f / frame->PageZoomFactor();
  return FloatPoint(frame_view->LayoutViewport()->GetScrollOffset())
      .ScaledBy(scale);
}

LayoutPoint PageToAbsolute(LocalFrame* frame, const FloatPoint& page_pos) {
  float scale_factor = frame ? frame->PageZoomFactor() : 1.0f;
  FloatPoint converted_point = page_pos.ScaledBy(scale_factor);

  if (frame && frame->View())
    converted_point = frame->View()->DocumentToFrame(converted_point);

  return LayoutPoint(converted_point);
}

}  // namespace

Touch::Touch(LocalFrame* frame,
             EventTarget* target,
             int identifier,
             const FloatPoint& screen_pos,
             const FloatPoint& page_pos,
             const FloatSize& radius,
             float rotation_angle,
             float force,
             String region)
    : target_(target),
      identifier_(identifier),
      client_pos_(page_pos - ContentsOffset(frame)),
      screen_pos_(screen_pos),
      page_pos_(page_pos),
      radius_(radius),
      rotation_angle_(rotation_angle),
      force_(force),
      region_(region),
      absolute_location_(PageToAbsolute(frame, page_pos)) {}

Touch::Touch(EventTarget* target,
             int identifier,
             const FloatPoint& client_pos,
             const FloatPoint& screen_pos,
             const FloatPoint& page_pos,
             const FloatSize& radius,
             float rotation_angle,
             float force,
             String region,
             LayoutPoint absolute_location)
    : target_(target),
      identifier_(identifier),
      client_pos_(client_pos),
      screen_pos_(screen_pos),
      page_pos_(page_pos),
      radius_(radius),
      rotation_angle_(rotation_angle),
      force_(force),
      region_(region),
      absolute_location_(absolute_location) {}

Touch::Touch(LocalFrame* frame, const TouchInit* initializer)
    : target_(initializer->target()),
      identifier_(initializer->identifier()),
      client_pos_(FloatPoint(initializer->clientX(), initializer->clientY())),
      screen_pos_(FloatPoint(initializer->screenX(), initializer->screenY())),
      page_pos_(FloatPoint(initializer->pageX(), initializer->pageY())),
      radius_(FloatSize(initializer->radiusX(), initializer->radiusY())),
      rotation_angle_(initializer->rotationAngle()),
      force_(initializer->force()),
      region_(initializer->region()),
      absolute_location_(PageToAbsolute(frame, page_pos_)) {}

Touch* Touch::CloneWithNewTarget(EventTarget* event_target) const {
  return MakeGarbageCollected<Touch>(
      event_target, identifier_, client_pos_, screen_pos_, page_pos_, radius_,
      rotation_angle_, force_, region_, absolute_location_);
}

void Touch::Trace(Visitor* visitor) {
  visitor->Trace(target_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
