// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorMutableState_h
#define CompositorMutableState_h

#include "platform/PlatformExport.h"

class SkMatrix44;

namespace cc {
class LayerImpl;
}  // namespace cc

namespace blink {

class CompositorMutation;

// This class wraps the compositor-owned, mutable state for a single element.
class PLATFORM_EXPORT CompositorMutableState {
 public:
  CompositorMutableState(CompositorMutation*,
                         cc::LayerImpl* main,
                         cc::LayerImpl* scroll);
  ~CompositorMutableState();

  double Opacity() const;
  void SetOpacity(double);

  const SkMatrix44& Transform() const;
  void SetTransform(const SkMatrix44&);

  float ScrollLeft() const;
  void SetScrollLeft(float);

  float ScrollTop() const;
  void SetScrollTop(float);

 private:
  CompositorMutation* mutation_;
  cc::LayerImpl* main_layer_;
  cc::LayerImpl* scroll_layer_;
};

}  // namespace blink

#endif  // CompositorMutableState_h
