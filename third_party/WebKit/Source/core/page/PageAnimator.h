// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PageAnimator_h
#define PageAnimator_h

#include "core/CoreExport.h"
#include "core/animation/AnimationClock.h"
#include "platform/heap/Handle.h"

namespace blink {

class LocalFrame;
class Page;

class CORE_EXPORT PageAnimator final : public GarbageCollected<PageAnimator> {
 public:
  static PageAnimator* create(Page&);
  DECLARE_TRACE();
  void scheduleVisualUpdate(LocalFrame*);
  void serviceScriptedAnimations(double monotonicAnimationStartTime);

  bool isServicingAnimations() const { return m_servicingAnimations; }

  // TODO(alancutter): Remove the need for this by implementing frame request
  // suppression logic at the BeginMainFrame level. This is a temporary
  // workaround to fix a perf regression.
  // DO NOT use this outside of crbug.com/704763.
  void setSuppressFrameRequestsWorkaroundFor704763Only(bool);

  // See documents of methods with the same names in FrameView class.
  void updateAllLifecyclePhases(LocalFrame& rootFrame);
  AnimationClock& clock() { return m_animationClock; }

 private:
  explicit PageAnimator(Page&);

  Member<Page> m_page;
  bool m_servicingAnimations;
  bool m_updatingLayoutAndStyleForPainting;
  bool m_suppressFrameRequestsWorkaroundFor704763Only = false;
  AnimationClock m_animationClock;
};

}  // namespace blink

#endif  // PageAnimator_h
