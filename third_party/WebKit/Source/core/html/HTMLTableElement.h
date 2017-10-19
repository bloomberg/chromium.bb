/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#ifndef HTMLTableElement_h
#define HTMLTableElement_h

#include "core/CoreExport.h"
#include "core/html/HTMLElement.h"

namespace blink {

class ExceptionState;
class HTMLCollection;
class HTMLTableCaptionElement;
class HTMLTableRowsCollection;
class HTMLTableSectionElement;

class CORE_EXPORT HTMLTableElement final : public HTMLElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  DECLARE_NODE_FACTORY(HTMLTableElement);

  HTMLTableCaptionElement* caption() const;
  void setCaption(HTMLTableCaptionElement*, ExceptionState&);

  HTMLTableSectionElement* tHead() const;
  void setTHead(HTMLTableSectionElement*, ExceptionState&);

  HTMLTableSectionElement* tFoot() const;
  void setTFoot(HTMLTableSectionElement*, ExceptionState&);

  HTMLTableSectionElement* createTHead();
  void deleteTHead();
  HTMLTableSectionElement* createTFoot();
  void deleteTFoot();
  HTMLTableSectionElement* createTBody();
  HTMLTableCaptionElement* createCaption();
  void deleteCaption();
  HTMLTableRowElement* insertRow(int index, ExceptionState&);
  void deleteRow(int index, ExceptionState&);

  HTMLTableRowsCollection* rows();
  HTMLCollection* tBodies();

  const AtomicString& Rules() const;
  const AtomicString& Summary() const;

  const StylePropertySet* AdditionalCellStyle();
  const StylePropertySet* AdditionalGroupStyle(bool rows);

  virtual void Trace(blink::Visitor*);

 private:
  explicit HTMLTableElement(Document&);
  ~HTMLTableElement();

  void ParseAttribute(const AttributeModificationParams&) override;
  bool IsPresentationAttribute(const QualifiedName&) const override;
  void CollectStyleForPresentationAttribute(const QualifiedName&,
                                            const AtomicString&,
                                            MutableStylePropertySet*) override;
  bool IsURLAttribute(const Attribute&) const override;
  bool HasLegalLinkAttribute(const QualifiedName&) const override;
  const QualifiedName& SubResourceAttributeName() const override;

  // Used to obtain either a solid or outset border decl and to deal with the
  // frame and rules attributes.
  const StylePropertySet* AdditionalPresentationAttributeStyle() override;

  enum TableRules {
    kUnsetRules,
    kNoneRules,
    kGroupsRules,
    kRowsRules,
    kColsRules,
    kAllRules
  };
  enum CellBorders {
    kNoBorders,
    kSolidBorders,
    kInsetBorders,
    kSolidBordersColsOnly,
    kSolidBordersRowsOnly
  };

  CellBorders GetCellBorders() const;

  StylePropertySet* CreateSharedCellStyle();

  HTMLTableSectionElement* LastBody() const;

  void SetNeedsTableStyleRecalc() const;

  // Sets a precise border width and creates an outset border for the table and
  // for its cells.
  bool border_attr_;
  // Overrides the outset border and makes it solid for the table and cells
  // instead.
  bool border_color_attr_;
  // Implies a thin border width if no border is set and then a certain set of
  // solid/hidden borders based off the value.
  bool frame_attr_;
  // Implies a thin border width, a collapsing border model, and all borders on
  // the table becoming set to hidden (if frame/border are present, to none
  // otherwise).
  TableRules rules_attr_;

  unsigned short padding_;
  Member<StylePropertySet> shared_cell_style_;
};

}  // namespace blink

#endif  // HTMLTableElement_h
