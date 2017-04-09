/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef Fullscreen_h
#define Fullscreen_h

#include "core/CoreExport.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "platform/Supplementable.h"
#include "platform/Timer.h"
#include "platform/geometry/LayoutRect.h"
#include "wtf/Deque.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace blink {

class ComputedStyle;
class LayoutFullScreen;

class CORE_EXPORT Fullscreen final
    : public GarbageCollectedFinalized<Fullscreen>,
      public Supplement<Document>,
      public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(Fullscreen);

 public:
  virtual ~Fullscreen();
  static const char* SupplementName();
  static Fullscreen& From(Document&);
  static Fullscreen* FromIfExists(Document&);
  static Element* FullscreenElementFrom(Document&);
  static Element* FullscreenElementForBindingFrom(TreeScope&);
  static Element* CurrentFullScreenElementFrom(Document&);
  static Element* CurrentFullScreenElementForBindingFrom(Document&);
  static bool IsCurrentFullScreenElement(const Element&);

  enum class RequestType {
    // Element.requestFullscreen()
    kUnprefixed,
    // Element.webkitRequestFullscreen()/webkitRequestFullScreen() and
    // HTMLVideoElement.webkitEnterFullscreen()/webkitEnterFullScreen()
    kPrefixed,
  };

  static void RequestFullscreen(Element&);

  // |forCrossProcessDescendant| is used in OOPIF scenarios and is set to
  // true when fullscreen is requested for an out-of-process descendant
  // element.
  static void RequestFullscreen(Element&,
                                RequestType,
                                bool for_cross_process_descendant = false);

  static void FullyExitFullscreen(Document&);
  static void ExitFullscreen(Document&);

  static bool FullscreenEnabled(Document&);
  // TODO(foolip): The fullscreen element stack is modified synchronously in
  // requestFullscreen(), which is not per spec and means that
  // |fullscreenElement()| is not always the same as
  // |currentFullScreenElement()|, see https://crbug.com/402421.
  Element* FullscreenElement() const {
    return !fullscreen_element_stack_.IsEmpty()
               ? fullscreen_element_stack_.back().first.Get()
               : nullptr;
  }

  // Called by FullscreenController to notify that we've entered or exited
  // fullscreen. All frames are notified, so there may be no pending request.
  void DidEnterFullscreen();
  void DidExitFullscreen();

  void SetFullScreenLayoutObject(LayoutFullScreen*);
  LayoutFullScreen* FullScreenLayoutObject() const {
    return full_screen_layout_object_;
  }
  void FullScreenLayoutObjectDestroyed();

  void ElementRemoved(Element&);

  // Returns true if the current fullscreen element stack corresponds to a
  // container for an actual fullscreen element in a descendant
  // out-of-process iframe.
  bool ForCrossProcessDescendant() { return for_cross_process_descendant_; }

  // Mozilla API
  // TODO(foolip): |currentFullScreenElement()| is a remnant from before the
  // fullscreen element stack. It is still maintained separately from the
  // stack and is is what the :-webkit-full-screen pseudo-class depends on. It
  // should be removed, see https://crbug.com/402421.
  Element* CurrentFullScreenElement() const {
    return current_full_screen_element_.Get();
  }

  // ContextLifecycleObserver:
  void ContextDestroyed(ExecutionContext*) override;

  DECLARE_VIRTUAL_TRACE();

 private:
  static Fullscreen* FromIfExistsSlow(Document&);

  explicit Fullscreen(Document&);

  Document* GetDocument();

  void ClearFullscreenElementStack();
  void PopFullscreenElementStack();
  void PushFullscreenElementStack(Element&, RequestType);

  void EnqueueChangeEvent(Document&, RequestType);
  void EnqueueErrorEvent(Element&, RequestType);
  void EventQueueTimerFired(TimerBase*);

  Member<Element> pending_fullscreen_element_;
  HeapVector<std::pair<Member<Element>, RequestType>> fullscreen_element_stack_;
  Member<Element> current_full_screen_element_;
  LayoutFullScreen* full_screen_layout_object_;
  TaskRunnerTimer<Fullscreen> event_queue_timer_;
  HeapDeque<Member<Event>> event_queue_;
  LayoutRect saved_placeholder_frame_rect_;
  RefPtr<ComputedStyle> saved_placeholder_computed_style_;

  // TODO(alexmos, dcheng): Currently, this assumes that if fullscreen was
  // entered for an element in an out-of-process iframe, then it's not
  // possible to re-enter fullscreen for a different element in this
  // document, since that requires a user gesture, which can't be obtained
  // since nothing in this document is visible, and since user gestures can't
  // be forwarded across processes. However, the latter assumption could
  // change if https://crbug.com/161068 is fixed so that cross-process
  // postMessage can carry user gestures.  If that happens, this should be
  // moved to be part of |m_fullscreenElementStack|.
  bool for_cross_process_descendant_;
};

inline Fullscreen* Fullscreen::FromIfExists(Document& document) {
  if (!document.HasFullscreenSupplement())
    return nullptr;
  return FromIfExistsSlow(document);
}

inline bool Fullscreen::IsCurrentFullScreenElement(const Element& element) {
  if (Fullscreen* found = FromIfExists(element.GetDocument()))
    return found->CurrentFullScreenElement() == &element;
  return false;
}

}  // namespace blink

#endif  // Fullscreen_h
