// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// This plugin is intended to be used in a telemetry test for tracing touch
// input latency. It is a simple touch drawing app, that for each touch move
// event, it draws a square with fix size.
// When the plugin instance is initialized, we call
// InputEventPrivate::StartTrackingLatency to enable latency tracking.
// And for each touch move event, we call
// InputEventPrivate::TraceInputLatency(true) to indicate the touch event
// causes rendering effect and its input latency should be tracked.
// The plugin is built as a pexe and bundled with a telemetry test page.
// For how to build the pexe, see the accompanying README file.

#include <algorithm>

#include "ppapi/c/pp_input_event.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/private/input_event_private.h"
#include "ppapi/cpp/size.h"
#include "ppapi/cpp/view.h"
#include "ppapi/utility/graphics/paint_manager.h"

pp::Rect SquareForTouchPoint(int x, int y) {
  return PP_MakeRectFromXYWH(x - 30, y - 30,
                             30 * 2 + 1, 30 * 2 + 1);
}

static void FillRect(pp::ImageData* image,
                     int left,
                     int top,
                     int width,
                     int height,
                     uint32_t color) {
  for (int y = std::max(0, top);
       y < std::min(image->size().height() - 1, top + height);
       y++) {
    for (int x = std::max(0, left);
         x < std::min(image->size().width() - 1, left + width);
         x++) {
      *image->GetAddr32(pp::Point(x, y)) = color;
    }
  }
}

class MyInstance : public pp::Instance, public pp::PaintManager::Client {
 public:
  explicit MyInstance(PP_Instance instance)
      : pp::Instance(instance),
        paint_manager_() {
    paint_manager_.Initialize(this, this, false);
    RequestInputEvents(PP_INPUTEVENT_CLASS_TOUCH);
    pp::InputEventPrivate::StartTrackingLatency(pp::InstanceHandle(instance));
  }

  virtual bool HandleInputEvent(const pp::InputEvent& event) {
    switch (event.GetType()) {
      case PP_INPUTEVENT_TYPE_TOUCHSTART:
      case PP_INPUTEVENT_TYPE_TOUCHEND:
      case PP_INPUTEVENT_TYPE_TOUCHCANCEL: {
        pp::InputEventPrivate private_event(event);
        private_event.TraceInputLatency(false);
        return true;
      }

      case PP_INPUTEVENT_TYPE_TOUCHMOVE: {
        pp::TouchInputEvent touch(event);
        uint32_t count = touch.GetTouchCount(PP_TOUCHLIST_TYPE_CHANGEDTOUCHES);
        if (count > 0) {
          pp::TouchPoint point = touch.GetTouchByIndex(
              PP_TOUCHLIST_TYPE_CHANGEDTOUCHES, 0);
          UpdateSquareTouch(static_cast<int>(point.position().x()),
                            static_cast<int>(point.position().y()));
          pp::InputEventPrivate private_event(event);
          private_event.TraceInputLatency(true);
        } else {
          pp::InputEventPrivate private_event(event);
          private_event.TraceInputLatency(false);
        }
        return true;
      }
      default:
        return false;
    }
  }

  virtual void DidChangeView(const pp::View& view) {
    paint_manager_.SetSize(view.GetRect().size());
  }

  // PaintManager::Client implementation.
  virtual bool OnPaint(pp::Graphics2D& graphics_2d,
                       const std::vector<pp::Rect>& paint_rects,
                       const pp::Rect& paint_bounds) {
    pp::ImageData updated_image(this, PP_IMAGEDATAFORMAT_BGRA_PREMUL,
                                paint_bounds.size(), false);

    for (size_t i = 0; i < paint_rects.size(); i++) {
      // Since our image is just the invalid region, we need to offset the
      // areas we paint by that much. This is just a light blue background.
      FillRect(&updated_image,
               paint_rects[i].x(),
               paint_rects[i].y(),
               paint_rects[i].width(),
               paint_rects[i].height(),
               0xFF000000);
    }

    graphics_2d.PaintImageData(updated_image, paint_bounds.point());
    return true;
  }

 private:
  void UpdateSquareTouch(int x, int y) {
    paint_manager_.InvalidateRect(SquareForTouchPoint(x, y));
  }

  pp::PaintManager paint_manager_;
};

class MyModule : public pp::Module {
 public:
  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new MyInstance(instance);
  }
};

namespace pp {

Module* CreateModule() {
  return new MyModule();
}

}  // namespace pp
