/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2007, 2010 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef HTMLMarqueeElement_h
#define HTMLMarqueeElement_h

#include "core/animation/Animation.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/html/HTMLElement.h"

namespace blink {

class HTMLMarqueeElement final : public HTMLElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  DECLARE_VIRTUAL_TRACE();

  static HTMLMarqueeElement* create(Document&);

  InsertionNotificationRequest insertedInto(ContainerNode*) final;
  void removedFrom(ContainerNode*) final;

  bool isHorizontal() const;

  int scrollAmount() const;
  void setScrollAmount(int, ExceptionState&);

  int scrollDelay() const;
  void setScrollDelay(int, ExceptionState&);

  int loop() const;
  void setLoop(int, ExceptionState&);

  void start();
  void stop();

 private:
  explicit HTMLMarqueeElement(Document&);
  void didAddUserAgentShadowRoot(ShadowRoot&) override;

  bool isPresentationAttribute(const QualifiedName&) const override;
  void collectStyleForPresentationAttribute(const QualifiedName&,
                                            const AtomicString&,
                                            MutableStylePropertySet*) override;

  class RequestAnimationFrameCallback;
  class AnimationFinished;

  struct AnimationParameters {
    String transformBegin;
    String transformEnd;
    double distance;
  };

  struct Metrics {
    double contentWidth;
    double contentHeight;
    double marqueeWidth;
    double marqueeHeight;
  };

  StringKeyframeEffectModel* createEffectModel(const AnimationParameters&);

  void continueAnimation();
  bool shouldContinue();

  enum Behavior { kScroll, kSlide, kAlternate };
  Behavior getBehavior() const;

  enum Direction { kLeft, kRight, kUp, kDown };
  Direction getDirection() const;

  Metrics getMetrics();
  AnimationParameters getAnimationParameters();
  AtomicString createTransform(double value) const;

  static const int kDefaultScrollAmount = 6;
  static const int kDefaultScrollDelayMS = 85;
  static const int kMinimumScrollDelayMS = 60;
  static const int kDefaultLoopLimit = -1;

  int m_continueCallbackRequestId = 0;
  int m_loopCount = 0;
  Member<Element> m_mover;
  Member<Animation> m_player;
};

}  // namespace blink

#endif  // HTMLMarqueeElement_h
