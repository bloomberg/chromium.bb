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
#include "core/dom/FrameRequestCallback.h"
#include "core/html/HTMLElement.h"
#include "wtf/Noncopyable.h"

namespace blink {

class HTMLMarqueeElement final : public HTMLElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  DECLARE_VIRTUAL_TRACE();

  static HTMLMarqueeElement* create(Document&);

  void attributeChanged(const QualifiedName&,
                        const AtomicString& oldValue,
                        const AtomicString& newValue,
                        AttributeModificationReason) final;
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

  class RequestAnimationFrameCallback final : public FrameRequestCallback {
    WTF_MAKE_NONCOPYABLE(RequestAnimationFrameCallback);

   public:
    explicit RequestAnimationFrameCallback(HTMLMarqueeElement* marquee)
        : m_marquee(marquee) {}
    void handleEvent(double) override;

    DEFINE_INLINE_VIRTUAL_TRACE() {
      visitor->trace(m_marquee);
      FrameRequestCallback::trace(visitor);
    }

   private:
    Member<HTMLMarqueeElement> m_marquee;
  };

  class AnimationFinished final : public EventListener {
    WTF_MAKE_NONCOPYABLE(AnimationFinished);

   public:
    explicit AnimationFinished(HTMLMarqueeElement* marquee)
        : EventListener(CPPEventListenerType), m_marquee(marquee) {}

    bool operator==(const EventListener& that) const override {
      return this == &that;
    }

    void handleEvent(ExecutionContext*, Event*) override;

    DEFINE_INLINE_VIRTUAL_TRACE() {
      visitor->trace(m_marquee);
      EventListener::trace(visitor);
    }

   private:
    Member<HTMLMarqueeElement> m_marquee;
  };

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

  void attributeChangedCallback(const QualifiedName& attr,
                                const String& newValue);

  StringKeyframeEffectModel* createEffectModel(AnimationParameters&);

  void continueAnimation();
  bool shouldContinue();

  enum Behavior { Scroll, Slide, Alternate };
  Behavior behavior() const;

  enum Direction { Left, Right, Up, Down };
  Direction direction() const;

  bool trueSpeed() const;

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
