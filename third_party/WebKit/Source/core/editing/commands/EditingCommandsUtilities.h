/*
 * Copyright (C) 2004, 2006, 2008 Apple Inc. All rights reserved.
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

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EditingCommandsUtilities_h
#define EditingCommandsUtilities_h

#include "core/CoreExport.h"
#include "core/editing/Forward.h"
#include "core/editing/TextGranularity.h"
#include "core/events/InputEvent.h"
#include "platform/wtf/text/CharacterNames.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

enum class DeleteDirection {
  kForward,
  kBackward,
};

class Document;
class Element;
class HTMLElement;
class Node;
class QualifiedName;

// -------------------------------------------------------------------------
// Node
// -------------------------------------------------------------------------

Node* HighestNodeToRemoveInPruning(Node*, const Node* exclude_node = nullptr);

Element* EnclosingTableCell(const Position&);
Node* EnclosingEmptyListItem(const VisiblePosition&);

bool IsTableStructureNode(const Node*);
bool IsNodeRendered(const Node&);
bool IsInline(const Node*);
// Returns true if specified nodes are elements, have identical tag names,
// have identical attributes, and are editable.
CORE_EXPORT bool AreIdenticalElements(const Node&, const Node&);

// -------------------------------------------------------------------------
// Position
// -------------------------------------------------------------------------

Position PositionBeforeContainingSpecialElement(
    const Position&,
    HTMLElement** containing_special_element = nullptr);
Position PositionAfterContainingSpecialElement(
    const Position&,
    HTMLElement** containing_special_element = nullptr);

bool LineBreakExistsAtPosition(const Position&);

// miscellaneous functions on Position

enum WhitespacePositionOption {
  kNotConsiderNonCollapsibleWhitespace,
  kConsiderNonCollapsibleWhitespace
};

// |leadingCollapsibleWhitespacePosition(position)| returns a previous position
// of |position| if it is at collapsible whitespace, otherwise it returns null
// position. When it is called with |NotConsiderNonCollapsibleWhitespace| and
// a previous position in a element which has CSS property "white-space:pre",
// or its variant, |leadingCollapsibleWhitespacePosition()| returns null
// position.
Position LeadingCollapsibleWhitespacePosition(
    const Position&,
    TextAffinity,
    WhitespacePositionOption = kNotConsiderNonCollapsibleWhitespace);

unsigned NumEnclosingMailBlockquotes(const Position&);

// -------------------------------------------------------------------------
// VisiblePosition
// -------------------------------------------------------------------------

bool LineBreakExistsAtVisiblePosition(const VisiblePosition&);

// -------------------------------------------------------------------------
// HTMLElement
// -------------------------------------------------------------------------

HTMLElement* CreateHTMLElement(Document&, const QualifiedName&);
HTMLElement* EnclosingList(const Node*);
HTMLElement* OutermostEnclosingList(const Node*,
                                    const HTMLElement* root_list = nullptr);
Node* EnclosingListChild(const Node*);

// -------------------------------------------------------------------------
// Element
// -------------------------------------------------------------------------

bool CanMergeLists(const Element& first_list, const Element& second_list);

// -------------------------------------------------------------------------
// VisibleSelection
// -------------------------------------------------------------------------

// Functions returning VisibleSelection
VisibleSelection SelectionForParagraphIteration(const VisibleSelection&);

const String& NonBreakingSpaceString();

CORE_EXPORT void TidyUpHTMLStructure(Document&);

InputEvent::InputType DeletionInputTypeFromTextGranularity(DeleteDirection,
                                                           TextGranularity);
}  // namespace blink

#endif
