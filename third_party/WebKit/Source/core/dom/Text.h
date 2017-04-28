/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights
 * reserved.
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

#ifndef Text_h
#define Text_h

#include "core/CoreExport.h"
#include "core/dom/CharacterData.h"

namespace blink {

class ExceptionState;
class LayoutText;

class CORE_EXPORT Text : public CharacterData {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const unsigned kDefaultLengthLimit = 1 << 16;

  static Text* Create(Document&, const String&);
  static Text* CreateEditingText(Document&, const String&);

  LayoutText* GetLayoutObject() const;

  // mergeNextSiblingNodesIfPossible() merges next sibling nodes if possible
  // then returns a node not merged.
  Node* MergeNextSiblingNodesIfPossible();
  Text* splitText(unsigned offset, ExceptionState&);

  // DOM Level 3: http://www.w3.org/TR/DOM-Level-3-Core/core.html#ID-1312295772

  String wholeText() const;
  Text* ReplaceWholeText(const String&);

  void RecalcTextStyle(StyleRecalcChange);
  void RebuildTextLayoutTree(Text* next_text_sibling);
  bool TextLayoutObjectIsNeeded(const ComputedStyle&,
                                const LayoutObject& parent) const;
  LayoutText* CreateTextLayoutObject(const ComputedStyle&);
  void UpdateTextLayoutObject(unsigned offset_of_replaced_data,
                              unsigned length_of_replaced_data);

  void AttachLayoutTree(const AttachContext& = AttachContext()) final;
  void ReattachLayoutTreeIfNeeded();

  bool CanContainRangeEndPoint() const final { return true; }
  NodeType getNodeType() const override;

  DECLARE_VIRTUAL_TRACE();

 protected:
  Text(TreeScope& tree_scope, const String& data, ConstructionType type)
      : CharacterData(tree_scope, data, type) {}

 private:
  String nodeName() const override;
  Node* cloneNode(bool deep, ExceptionState&) final;

  bool IsTextNode() const =
      delete;  // This will catch anyone doing an unnecessary check.

  bool NeedsWhitespaceLayoutObject();

  virtual Text* CloneWithData(const String&);
};

DEFINE_NODE_TYPE_CASTS(Text, IsTextNode());

}  // namespace blink

#endif  // Text_h
