/*
 * Copyright (C) 2013 Bloomberg L.P. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/editing/commands/block_command.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_names.h"

namespace blink {

static bool isTableCellOrRootEditable(const Node* node)
{
    return IsTableCell(node) || (IsRootEditableElement(*node));
}

BlockCommand::BlockCommand(Document& document)
    : CompositeEditCommand(document)
{
}

void BlockCommand::FormatBlockExtent(Node* prpFirstNode, Node* prpLastNode, Node* stayWithin, EditingState *editingState)
{
    Node* currentNode = prpFirstNode;
    Node* endNode = prpLastNode;

    while (currentNode->IsDescendantOf(endNode))
        endNode = endNode->lastChild();

    while (currentNode) {
        while (endNode->IsDescendantOf(currentNode)) {
            DCHECK(currentNode->firstChild());
            currentNode = currentNode->firstChild();
        }

        Node* firstSibling = currentNode;
        Node* lastSibling = currentNode;

        while (lastSibling != endNode) {
            Node* nextSibling = lastSibling->nextSibling();
            if (!nextSibling || endNode->IsDescendantOf(nextSibling))
                break;
            lastSibling = nextSibling;
        }

        Node* nextNode = lastSibling == endNode ? 0 : NodeTraversal::NextSkippingChildren(*lastSibling, stayWithin);
        FormatBlockSiblings(firstSibling, lastSibling, stayWithin, endNode, editingState);
        currentNode = nextNode;
    }
}

void BlockCommand::FormatBlockSiblings(Node* prpFirstSibling, Node* prpLastSibling, Node* stayWithin, Node* lastNode, EditingState *editingState)
{
    NOTREACHED();
}

void BlockCommand::DoApply(EditingState *editingState)
{
    VisiblePosition startOfSelection;
    VisiblePosition endOfSelection;
    ContainerNode* startScope;
    ContainerNode* endScope;
    int startIndex;
    int endIndex;

    if (!PrepareForBlockCommand(startOfSelection, endOfSelection, startScope, endScope, startIndex, endIndex, editingState))
        return;
    FormatSelection(startOfSelection, endOfSelection, editingState);
    FinishBlockCommand(startScope, endScope, startIndex, endIndex);
}

void BlockCommand::FormatSelection(const VisiblePosition& startOfSelection,
                                   const VisiblePosition& endOfSelection,
                                   EditingState *editingState)
{
    // might be null if the recursion below goes awry
    if (startOfSelection.IsNull() || endOfSelection.IsNull())
        return;

    Node* startEnclosingCell = EnclosingNodeOfType(startOfSelection.DeepEquivalent(), &IsTableCell);
    Node* endEnclosingCell = EnclosingNodeOfType(endOfSelection.DeepEquivalent(), &IsTableCell);

    if (startEnclosingCell != endEnclosingCell) {
        if (startEnclosingCell && (!endEnclosingCell || !endEnclosingCell->IsDescendantOf(startEnclosingCell))) {
            VisiblePosition newEnd = CreateVisiblePosition(
                    Position::LastPositionInNode(*startEnclosingCell));
            VisiblePosition nextStart = NextPositionOf(newEnd);
            while (IsDisplayInsideTable(nextStart.DeepEquivalent().AnchorNode()))
                nextStart = NextPositionOf(nextStart);
            // TODO: fix recursion!
            FormatSelection(startOfSelection, newEnd, editingState);
            FormatSelection(nextStart, endOfSelection, editingState);
            return;
        }

        DCHECK(endEnclosingCell);

        VisiblePosition nextStart = CreateVisiblePosition(PositionTemplate<EditingStrategy>::FirstPositionInNode(*endEnclosingCell));
        VisiblePosition newEnd = PreviousPositionOf(nextStart);
        while (IsDisplayInsideTable(newEnd.DeepEquivalent().AnchorNode()))
            newEnd = PreviousPositionOf(newEnd);
        // TODO: fix recursion!
        FormatSelection(startOfSelection, newEnd, editingState);
        FormatSelection(nextStart, endOfSelection, editingState);
        return;
    }


    Node* root = EnclosingNodeOfType(startOfSelection.DeepEquivalent(), &isTableCellOrRootEditable);
    if (!root || root == startOfSelection.DeepEquivalent().AnchorNode())
        return;

    Node* currentNode = BlockExtentStart(startOfSelection.DeepEquivalent().AnchorNode(), root);
    Node* endNode = BlockExtentEnd(endOfSelection.DeepEquivalent().AnchorNode(), root);

    while (currentNode->IsDescendantOf(endNode))
        endNode = endNode->lastChild();

    FormatBlockExtent(currentNode, endNode, root, editingState);
}

}
