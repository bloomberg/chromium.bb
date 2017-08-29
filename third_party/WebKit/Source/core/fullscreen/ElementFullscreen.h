// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ElementFullscreen_h
#define ElementFullscreen_h

#include "core/dom/events/EventTarget.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class Element;

class ElementFullscreen {
  STATIC_ONLY(ElementFullscreen);

 public:
  static void requestFullscreen(Element&);

  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(fullscreenchange);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(fullscreenerror);

  static void webkitRequestFullscreen(Element&);

  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(webkitfullscreenchange);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(webkitfullscreenerror);
};

}  // namespace blink

#endif  // ElementFullscreen_h
