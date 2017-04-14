/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "core/inspector/InspectorInputAgent.h"

#include "core/frame/FrameView.h"
#include "core/input/EventHandler.h"
#include "core/inspector/InspectedFrames.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "platform/geometry/FloatSize.h"
#include "platform/geometry/IntPoint.h"
#include "platform/geometry/IntRect.h"
#include "platform/geometry/IntSize.h"
#include "platform/wtf/CurrentTime.h"
#include "platform/wtf/Time.h"
#include "public/platform/WebTouchEvent.h"

namespace {

enum Modifiers {
  AltKey = 1 << 0,
  CtrlKey = 1 << 1,
  MetaKey = 1 << 2,
  ShiftKey = 1 << 3
};

unsigned GetEventModifiers(int modifiers) {
  unsigned platformModifiers = 0;
  if (modifiers & AltKey)
    platformModifiers |= blink::WebInputEvent::kAltKey;
  if (modifiers & CtrlKey)
    platformModifiers |= blink::WebInputEvent::kControlKey;
  if (modifiers & MetaKey)
    platformModifiers |= blink::WebInputEvent::kMetaKey;
  if (modifiers & ShiftKey)
    platformModifiers |= blink::WebInputEvent::kShiftKey;
  return platformModifiers;
}

// Convert given protocol timestamp which is in seconds since unix epoch to a
// platform event timestamp which is ticks since platform start. This conversion
// is an estimate because these two clocks respond differently to user setting
// time and NTP adjustments. If timestamp is empty then returns current
// monotonic timestamp.
TimeTicks GetEventTimeStamp(const blink::protocol::Maybe<double>& timestamp) {
  // Take a snapshot of difference between two clocks on first run and use it
  // for the duration of the application.
  static double epochToMonotonicTimeDelta =
      CurrentTime() - MonotonicallyIncreasingTime();
  if (timestamp.isJust()) {
    double ticksInSeconds = timestamp.fromJust() - epochToMonotonicTimeDelta;
    return TimeTicks::FromSeconds(ticksInSeconds);
  }
  return TimeTicks::Now();
}

class SyntheticInspectorTouchPoint : public blink::WebTouchPoint {
 public:
  SyntheticInspectorTouchPoint(int idParam,
                               State stateParam,
                               const blink::IntPoint& screenPos,
                               const blink::IntPoint& pos,
                               int radiusXParam,
                               int radiusYParam,
                               double rotationAngleParam,
                               double forceParam) {
    id = idParam;
    screen_position = screenPos;
    position = pos;
    state = stateParam;
    radius_x = radiusXParam;
    radius_y = radiusYParam;
    rotation_angle = rotationAngleParam;
    force = forceParam;
  }
};

class SyntheticInspectorTouchEvent : public blink::WebTouchEvent {
 public:
  SyntheticInspectorTouchEvent(const blink::WebInputEvent::Type type,
                               unsigned modifiers,
                               TimeTicks timestamp) {
    type_ = type;
    modifiers_ = modifiers;
    time_stamp_seconds_ = timestamp.InSeconds();
    moved_beyond_slop_region = true;
  }

  void append(const blink::WebTouchPoint& point) {
    if (touches_length < kTouchesLengthCap) {
      touches[touches_length] = point;
      touches_length++;
    }
  }
};

void ConvertInspectorPoint(blink::LocalFrame* frame,
                           const blink::IntPoint& pointInFrame,
                           blink::IntPoint* convertedPoint,
                           blink::IntPoint* globalPoint) {
  *convertedPoint = frame->View()->ConvertToRootFrame(pointInFrame);
  *globalPoint =
      frame->GetPage()
          ->GetChromeClient()
          .ViewportToScreen(blink::IntRect(pointInFrame, blink::IntSize(0, 0)),
                            frame->View())
          .Location();
}

}  // namespace

namespace blink {

using protocol::Response;

InspectorInputAgent::InspectorInputAgent(InspectedFrames* inspected_frames)
    : inspected_frames_(inspected_frames) {}

InspectorInputAgent::~InspectorInputAgent() {}

Response InspectorInputAgent::dispatchTouchEvent(
    const String& type,
    std::unique_ptr<protocol::Array<protocol::Input::TouchPoint>> touch_points,
    protocol::Maybe<int> modifiers,
    protocol::Maybe<double> timestamp) {
  WebInputEvent::Type converted_type;
  if (type == "touchStart")
    converted_type = WebInputEvent::kTouchStart;
  else if (type == "touchEnd")
    converted_type = WebInputEvent::kTouchEnd;
  else if (type == "touchMove")
    converted_type = WebInputEvent::kTouchMove;
  else
    return Response::Error(String("Unrecognized type: " + type));

  unsigned converted_modifiers = GetEventModifiers(modifiers.fromMaybe(0));

  SyntheticInspectorTouchEvent event(converted_type, converted_modifiers,
                                     GetEventTimeStamp(timestamp));

  int auto_id = 0;
  for (size_t i = 0; i < touch_points->length(); ++i) {
    protocol::Input::TouchPoint* point = touch_points->get(i);
    int radius_x = point->getRadiusX(1);
    int radius_y = point->getRadiusY(1);
    double rotation_angle = point->getRotationAngle(0.0);
    double force = point->getForce(1.0);
    int id;
    if (point->hasId()) {
      if (auto_id > 0)
        id = -1;
      else
        id = point->getId(0);
      auto_id = -1;
    } else {
      id = auto_id++;
    }
    if (id < 0) {
      return Response::Error(
          "All or none of the provided TouchPoints must supply positive "
          "integer ids.");
    }

    WebTouchPoint::State converted_state;
    String state = point->getState();
    if (state == "touchPressed")
      converted_state = WebTouchPoint::kStatePressed;
    else if (state == "touchReleased")
      converted_state = WebTouchPoint::kStateReleased;
    else if (state == "touchMoved")
      converted_state = WebTouchPoint::kStateMoved;
    else if (state == "touchStationary")
      converted_state = WebTouchPoint::kStateStationary;
    else if (state == "touchCancelled")
      converted_state = WebTouchPoint::kStateCancelled;
    else
      return Response::Error(String("Unrecognized state: " + state));

    // Some platforms may have flipped coordinate systems, but the given
    // coordinates assume the origin is in the top-left of the window. Convert.
    IntPoint converted_point, global_point;
    ConvertInspectorPoint(inspected_frames_->Root(),
                          IntPoint(point->getX(), point->getY()),
                          &converted_point, &global_point);

    SyntheticInspectorTouchPoint touch_point(
        id++, converted_state, global_point, converted_point, radius_x,
        radius_y, rotation_angle, force);
    event.append(touch_point);
  }

  // The generated touchpoints are in root frame co-ordinates
  // so set the scale to 1 and the the translation to zero.
  event.SetFrameScale(1);
  event.SetFrameTranslate(WebFloatPoint());

  // TODO: We need to add the support for generating coalesced events in
  // the devtools.
  Vector<WebTouchEvent> coalesced_events;

  inspected_frames_->Root()->GetEventHandler().HandleTouchEvent(
      event, coalesced_events);
  return Response::OK();
}

DEFINE_TRACE(InspectorInputAgent) {
  visitor->Trace(inspected_frames_);
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink
