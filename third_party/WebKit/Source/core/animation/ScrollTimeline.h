// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollTimeline_h
#define ScrollTimeline_h

#include "core/CoreExport.h"
#include "core/animation/AnimationTimeline.h"
#include "core/animation/ScrollTimelineOptions.h"
#include "core/dom/Element.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

// Implements the ScrollTimeline concept from the Scroll-linked Animations spec.
//
// A ScrollTimeline is a special form of AnimationTimeline whose time values are
// not determined by wall-clock time but instead the progress of scrolling in a
// scroll container. The user is able to specify which scroll container to
// track, the direction of scroll they care about, and various attributes to
// control the conversion of scroll amount to time output.
//
// Spec: https://wicg.github.io/scroll-animations/#scroll-timelines
class CORE_EXPORT ScrollTimeline final : public AnimationTimeline {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum ScrollDirection {
    Block,
    Inline,
  };

  static ScrollTimeline* Create(Document&,
                                ScrollTimelineOptions,
                                ExceptionState&);

  // AnimationTimeline implementation.
  double currentTime(bool& is_null) final;

  // IDL API implementation.
  Element* scrollSource();
  String orientation();
  void timeRange(DoubleOrScrollTimelineAutoKeyword&);

  virtual void Trace(blink::Visitor*);

 private:
  ScrollTimeline(const Document&, Element*, ScrollDirection, double);

  Member<Element> scroll_source_;
  ScrollDirection orientation_;
  double time_range_;
};

}  // namespace blink

#endif
