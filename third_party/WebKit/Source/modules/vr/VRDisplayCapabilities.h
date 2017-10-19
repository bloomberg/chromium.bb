// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VRDisplayCapabilities_h
#define VRDisplayCapabilities_h

#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"

namespace blink {

class VRDisplayCapabilities final
    : public GarbageCollected<VRDisplayCapabilities>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  VRDisplayCapabilities();

  bool hasPosition() const { return has_position_; }
  bool hasExternalDisplay() const { return has_external_display_; }
  bool canPresent() const { return can_present_; }
  unsigned maxLayers() const { return max_layers_; }

  void SetHasPosition(bool value) { has_position_ = value; }
  void SetHasExternalDisplay(bool value) { has_external_display_ = value; }
  void SetCanPresent(bool value) { can_present_ = value; }
  void SetMaxLayers(unsigned value) { max_layers_ = value; }

  virtual void Trace(blink::Visitor*);

 private:
  bool has_position_;
  bool has_external_display_;
  bool can_present_;
  unsigned max_layers_;
};

}  // namespace blink

#endif  // VRDisplayCapabilities_h
