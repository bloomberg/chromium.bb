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
  static const char* supplementName();
  static Fullscreen& from(Document&);
  static Fullscreen* fromIfExists(Document&);
  static Element* fullscreenElementFrom(Document&);
  static Element* fullscreenElementForBindingFrom(TreeScope&);
  static size_t fullscreenElementStackSizeFrom(Document&);
  static bool isFullscreenElement(const Element&);

  enum class RequestType {
    // Element.requestFullscreen()
    Unprefixed,
    // Element.webkitRequestFullscreen()/webkitRequestFullScreen() and
    // HTMLVideoElement.webkitEnterFullscreen()/webkitEnterFullScreen()
    Prefixed,
    // For WebRemoteFrameImpl to notify that a cross-process descendant frame
    // has requested and is about to enter fullscreen.
    PrefixedForCrossProcessDescendant,
  };

  static void requestFullscreen(Element&);
  static void requestFullscreen(Element&, RequestType);

  static void fullyExitFullscreen(Document&);

  enum class ExitType {
    // Exits fullscreen for one element in the document.
    Default,
    // Fully exits fullscreen for the document.
    Fully,
  };

  static void exitFullscreen(Document&, ExitType = ExitType::Default);

  static bool fullscreenEnabled(Document&);
  Element* fullscreenElement() const {
    return !m_fullscreenElementStack.isEmpty()
               ? m_fullscreenElementStack.back().first.get()
               : nullptr;
  }

  // Called by FullscreenController to notify that we've entered or exited
  // fullscreen. All frames are notified, so there may be no pending request.
  void didEnterFullscreen();
  void didExitFullscreen();

  void setFullScreenLayoutObject(LayoutFullScreen*);
  LayoutFullScreen* fullScreenLayoutObject() const {
    return m_fullScreenLayoutObject;
  }
  void fullScreenLayoutObjectDestroyed();

  void elementRemoved(Element&);

  // ContextLifecycleObserver:
  void contextDestroyed() override;

  DECLARE_VIRTUAL_TRACE();

 private:
  static Fullscreen* fromIfExistsSlow(Document&);

  explicit Fullscreen(Document&);

  Document* document();

  static void enqueueTaskForRequest(Document&,
                                    Element&,
                                    RequestType,
                                    bool error);
  static void runTaskForRequest(Document*, Element*, RequestType, bool error);

  static void enqueueTaskForExit(Document&, ExitType);
  static void runTaskForExit(Document*, ExitType);

  void clearFullscreenElementStack();
  void popFullscreenElementStack();
  void pushFullscreenElementStack(Element&, RequestType);
  void fullscreenElementChanged(Element* fromElement,
                                Element* toElement,
                                RequestType toRequestType);

  using ElementStackEntry = std::pair<Member<Element>, RequestType>;
  using ElementStack = HeapVector<ElementStackEntry>;
  ElementStack m_pendingRequests;
  ElementStack m_fullscreenElementStack;

  LayoutFullScreen* m_fullScreenLayoutObject;
  LayoutRect m_savedPlaceholderFrameRect;
  RefPtr<ComputedStyle> m_savedPlaceholderComputedStyle;
};

inline Fullscreen* Fullscreen::fromIfExists(Document& document) {
  if (!document.hasFullscreenSupplement())
    return nullptr;
  return fromIfExistsSlow(document);
}

inline bool Fullscreen::isFullscreenElement(const Element& element) {
  if (Fullscreen* found = fromIfExists(element.document()))
    return found->fullscreenElement() == &element;
  return false;
}

}  // namespace blink

#endif  // Fullscreen_h
