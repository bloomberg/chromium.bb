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

#include "third_party/blink/renderer/core/editing/commands/outdent_block_command.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/commands/editing_commands_utilities.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_names.h"

namespace blink {

static bool hasListAncestor(Node* node, Node* stayWithin)
{
    node = node->parentNode();
    while (node != stayWithin) {
        if (IsHTMLListElement(node))
            return true;
        node = node->parentNode();
    }
    return false;
}

static bool isIndentationBlock(Node* node, Node* stayWithin)
{
    if (IsHTMLListElement(node)) {
        return hasListAncestor(node, stayWithin);
    }
    return node->HasTagName(html_names::kBlockquoteTag);
}

static Node* FindCommonIndentationBlock(Node* firstSibling, Node* lastSibling, Node* stayWithin)
{
    DCHECK(firstSibling->IsDescendantOf(stayWithin));
    DCHECK(firstSibling->parentNode() == lastSibling->parentNode());

    Node* node = firstSibling == lastSibling ? firstSibling : firstSibling->parentNode();
    while (node != stayWithin) {
        if (isIndentationBlock(node, stayWithin))
            return node;
        node = node->parentNode();
    }
    return 0;
}

static bool hasVisibleChildren(Node* node)
{
    for (Node* child = node->firstChild(); child; child = child->nextSibling()) {
        if (child->GetLayoutObject())
            return true;
    }
    return false;
}

OutdentBlockCommand::OutdentBlockCommand(Document& document)
    : BlockCommand(document)
{
}

Node* OutdentBlockCommand::SplitStart(Node* ancestor, Node* prpChild)
{
    DCHECK(prpChild->IsDescendantOf(ancestor));

    Node* child = prpChild;

    while (child != ancestor) {
        Node* previous = PreviousRenderedSiblingExcludingWhitespace(child);
        if (previous)
            SplitElement(ToElement(child->parentNode()), previous->nextSibling());
        child = child->parentNode();
    }

    child = child->firstChild();
    return child;
}

Node* OutdentBlockCommand::SplitEnd(Node* ancestor, Node* prpChild)
{
    DCHECK(prpChild->IsDescendantOf(ancestor));

    Node* child = prpChild;
    bool reachedAncestor = false;

    while (!reachedAncestor) {
        reachedAncestor = child->parentNode() == ancestor;
        Node* next = NextRenderedSiblingExcludingWhitespace(child);
        if (next)
            SplitElement(ToElement(child->parentNode()), next);
        child = child->parentNode();
    }

    child = child->lastChild();
    return child;
}

void OutdentBlockCommand::OutdentSiblings(Node* prpFirstSibling, Node* prpLastSibling, Node* indentBlock, EditingState *editingState)
{
    DCHECK(indentBlock);
    if (!prpFirstSibling) {
        DCHECK(!prpLastSibling);
        DCHECK(!indentBlock->firstChild());
        RemoveNode(indentBlock, editingState);
        return;
    }

    DCHECK(prpFirstSibling->IsDescendantOf(indentBlock));
    DCHECK(prpFirstSibling->parentNode() == prpLastSibling->parentNode());

    Node* firstSibling = prpFirstSibling;
    Node* lastSibling = prpLastSibling;

    lastSibling = SplitEnd(indentBlock, lastSibling);
    indentBlock = lastSibling->parentNode();
    firstSibling = SplitStart(indentBlock, firstSibling);
    DCHECK(firstSibling->parentNode() == indentBlock);

    Node* current = firstSibling;
    Node* end = lastSibling->nextSibling();
    while (current != end) {
        Node* next = current->nextSibling();
        RemoveNode(current, editingState);
        InsertNodeBefore(current, indentBlock, editingState);
        current = next;
    }

    if (!hasVisibleChildren(indentBlock)) {
        RemoveNode(indentBlock, editingState);
    }
}

void OutdentBlockCommand::FormatBlockSiblings(Node* prpFirstSibling, Node* prpLastSibling, Node* stayWithin, Node* lastNode, EditingState *editingState)
{
    DCHECK(prpFirstSibling);
    DCHECK(prpLastSibling);
    DCHECK(prpFirstSibling->parentNode());
    DCHECK(prpFirstSibling->parentNode() == prpLastSibling->parentNode());
    DCHECK(prpFirstSibling->IsDescendantOf(stayWithin));

    Node* firstSibling = prpFirstSibling;
    Node* lastSibling = prpLastSibling;

    Node* indentBlock = FindCommonIndentationBlock(firstSibling, lastSibling, stayWithin);
    if (indentBlock) {
        if (indentBlock == firstSibling) {
            DCHECK(indentBlock == lastSibling);
            RemoveNodePreservingChildren(firstSibling, editingState);
        }
        else
            OutdentSiblings(firstSibling, lastSibling, indentBlock, editingState);
        return;
    }

    // If there is no common indent block, then look to see if any of
    // the siblings, or their children, are themselves indent blocks,
    // and if so, remove them to remove the indentation
    Node* current = firstSibling;
    Node* end = NodeTraversal::NextSkippingChildren(*lastSibling, stayWithin);
    while (current != end) {
        Node* next;
        if (isIndentationBlock(current, stayWithin)) {
            next = NodeTraversal::NextSkippingChildren(*current, stayWithin);
            RemoveNodePreservingChildren(current, editingState);
        }
        else
            next = NodeTraversal::Next(*current, stayWithin);
        current = next;
    }
}

}
