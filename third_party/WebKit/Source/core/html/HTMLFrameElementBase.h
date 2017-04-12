/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2004, 2006, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef HTMLFrameElementBase_h
#define HTMLFrameElementBase_h

#include "core/CoreExport.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "public/platform/WebFocusType.h"

namespace blink {

class CORE_EXPORT HTMLFrameElementBase : public HTMLFrameOwnerElement {
 public:
  bool CanContainRangeEndPoint() const final { return false; }

  // FrameOwner overrides:
  ScrollbarMode ScrollingMode() const final { return scrolling_mode_; }
  int MarginWidth() const final { return margin_width_; }
  int MarginHeight() const final { return margin_height_; }

 protected:
  HTMLFrameElementBase(const QualifiedName&, Document&);

  void ParseAttribute(const AttributeModificationParams&) override;
  InsertionNotificationRequest InsertedInto(ContainerNode*) override;
  void DidNotifySubtreeInsertionsToDocument() final;
  void AttachLayoutTree(const AttachContext& = AttachContext()) override;

  void SetScrollingMode(ScrollbarMode);
  void SetMarginWidth(int);
  void SetMarginHeight(int);

 private:
  bool SupportsFocus() const final;
  void SetFocused(bool, WebFocusType) final;

  bool IsURLAttribute(const Attribute&) const final;
  bool HasLegalLinkAttribute(const QualifiedName&) const final;
  bool IsHTMLContentAttribute(const Attribute&) const final;

  bool AreAuthorShadowsAllowed() const final { return false; }

  void SetLocation(const String&);
  void SetNameAndOpenURL();
  bool IsURLAllowed() const;
  void OpenURL(bool replace_current_item = true);

  ScrollbarMode scrolling_mode_;
  int margin_width_;
  int margin_height_;

  AtomicString url_;
  AtomicString frame_name_;
};

inline bool IsHTMLFrameElementBase(const HTMLElement& element) {
  return isHTMLFrameElement(element) || isHTMLIFrameElement(element);
}

DEFINE_HTMLELEMENT_TYPE_CASTS_WITH_FUNCTION(HTMLFrameElementBase);

}  // namespace blink

#endif  // HTMLFrameElementBase_h
