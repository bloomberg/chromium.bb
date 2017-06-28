/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FocusController_h
#define FocusController_h

#include "core/CoreExport.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/RefPtr.h"
#include "public/platform/WebFocusType.h"

namespace blink {

struct FocusCandidate;
struct FocusParams;
class Document;
class Element;
class FocusChangedObserver;
class Frame;
class HTMLFrameOwnerElement;
class InputDeviceCapabilities;
class LocalFrame;
class Node;
class Page;
class RemoteFrame;

class CORE_EXPORT FocusController final
    : public GarbageCollected<FocusController> {
  WTF_MAKE_NONCOPYABLE(FocusController);

 public:
  static FocusController* Create(Page*);

  void SetFocusedFrame(Frame*, bool notify_embedder = true);
  void FocusDocumentView(Frame*, bool notify_embedder = true);
  LocalFrame* FocusedFrame() const;
  Frame* FocusedOrMainFrame() const;

  // Finds the focused HTMLFrameOwnerElement, if any, in the provided frame.
  // An HTMLFrameOwnerElement is considered focused if the frame it owns, or
  // one of its descendant frames, is currently focused.
  HTMLFrameOwnerElement* FocusedFrameOwnerElement(
      LocalFrame& current_frame) const;

  // Determines whether the provided Document has focus according to
  // http://www.w3.org/TR/html5/editing.html#dom-document-hasfocus
  bool IsDocumentFocused(const Document&) const;

  bool SetInitialFocus(WebFocusType);
  bool AdvanceFocus(WebFocusType type,
                    InputDeviceCapabilities* source_capabilities = nullptr) {
    return AdvanceFocus(type, false, source_capabilities);
  }
  bool AdvanceFocusAcrossFrames(
      WebFocusType,
      RemoteFrame* from,
      LocalFrame* to,
      InputDeviceCapabilities* source_capabilities = nullptr);
  Element* FindFocusableElementInShadowHost(const Element& shadow_host);

  bool SetFocusedElement(Element*, Frame*, const FocusParams&);
  // |setFocusedElement| variant with SelectionBehaviorOnFocus::None,
  // |WebFocusTypeNone, and null InputDeviceCapabilities.
  bool SetFocusedElement(Element*, Frame*);

  void SetActive(bool);
  bool IsActive() const { return is_active_; }

  void SetFocused(bool);
  bool IsFocused() const { return is_focused_; }

  void RegisterFocusChangedObserver(FocusChangedObserver*);

  DECLARE_TRACE();

 private:
  explicit FocusController(Page*);

  Element* FindFocusableElement(WebFocusType, Element&);

  bool AdvanceFocus(WebFocusType,
                    bool initial_focus,
                    InputDeviceCapabilities* source_capabilities = nullptr);
  bool AdvanceFocusDirectionally(WebFocusType);
  bool AdvanceFocusInDocumentOrder(
      LocalFrame*,
      Element* start,
      WebFocusType,
      bool initial_focus,
      InputDeviceCapabilities* source_capabilities);

  bool AdvanceFocusDirectionallyInContainer(Node* container,
                                            const LayoutRect& starting_rect,
                                            WebFocusType);
  void FindFocusCandidateInContainer(Node& container,
                                     const LayoutRect& starting_rect,
                                     WebFocusType,
                                     FocusCandidate& closest);

  void NotifyFocusChangedObservers() const;

  Member<Page> page_;
  Member<Frame> focused_frame_;
  bool is_active_;
  bool is_focused_;
  bool is_changing_focused_frame_;
  HeapHashSet<WeakMember<FocusChangedObserver>> focus_changed_observers_;
};

}  // namespace blink

#endif  // FocusController_h
