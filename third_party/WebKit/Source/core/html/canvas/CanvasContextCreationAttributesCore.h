// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CanvasContextCreationAttributesCore_h
#define CanvasContextCreationAttributesCore_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/WTFString.h"
#include "third_party/WebKit/Source/core/dom/events/EventTarget.h"

namespace blink {

class CORE_EXPORT CanvasContextCreationAttributesCore {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  CanvasContextCreationAttributesCore();
  CanvasContextCreationAttributesCore(
      blink::CanvasContextCreationAttributesCore const&);
  virtual ~CanvasContextCreationAttributesCore();

  bool alpha = true;
  bool antialias = true;
  String color_space = "srgb";
  bool depth = true;
  bool fail_if_major_performance_caveat = false;
  bool low_latency = false;
  String pixel_format = "8-8-8-8";
  bool premultiplied_alpha = true;
  bool preserve_drawing_buffer = false;
  bool stencil = false;

  // This attribute is of type XRDevice, defined in modules/xr/XRDevice.h
  Member<EventTargetWithInlineData> compatible_xr_device;

  void Trace(blink::Visitor*);
};

}  // namespace blink

#endif  // CanvasContextCreationAttributes_h
