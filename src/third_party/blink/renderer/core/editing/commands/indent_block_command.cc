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

#include "third_party/blink/renderer/core/editing/commands/indent_block_command.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/commands/editing_commands_utilities.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_names.h"

namespace blink {

static const HTMLQualifiedName& getBlockQuoteName(const Node* parent)
{
    if (IsHTMLUListElement(parent))
        return html_names::kUlTag;
    else if (IsHTMLOListElement(parent))
        return html_names::kOlTag;
    else
        return html_names::kBlockquoteTag;
}

IndentBlockCommand::IndentBlockCommand(Document& document)
    : BlockCommand(document)
{
}

Element* IndentBlockCommand::CreateIndentBlock(const QualifiedName& tagName) const
{
    Element* element = CreateHTMLElement(GetDocument(), tagName);
    if (tagName.Matches(html_names::kBlockquoteTag))
        element->setAttribute(html_names::kStyleAttr, "margin: 0 0 0 40px; border: none; padding: 0px;");
    return element;
}

void IndentBlockCommand::IndentSiblings(Node* prpFirstSibling, Node* prpLastSibling, Node* lastNode, EditingState *editingState)
{
    Node* firstSibling = prpFirstSibling;
    Node* lastSibling = prpLastSibling;

    Element* blockForIndent = nullptr;
    Node* refChild = nullptr;
    bool needToMergeNextSibling = false;

    const blink::HTMLQualifiedName blockQName
        = getBlockQuoteName(firstSibling->parentNode());

    Node* previousSibling = PreviousRenderedSiblingExcludingWhitespace(firstSibling);
    if (previousSibling && previousSibling->IsElementNode()
            && ToElement(previousSibling)->HasTagName(blockQName)) {
        blockForIndent = ToElement(previousSibling);
        firstSibling = previousSibling->nextSibling();
    }

    Node* nextSibling = NextRenderedSiblingExcludingWhitespace(lastSibling);
    if (nextSibling && nextSibling->HasTagName(blockQName) && !lastNode->IsDescendantOf(nextSibling)) {
        if (!blockForIndent) {
            blockForIndent = ToElement(nextSibling);
            refChild = nextSibling->firstChild();
        }
        else if (nextSibling->firstChild())
            needToMergeNextSibling = true;
        lastSibling = nextSibling->previousSibling();
    }

    if (!blockForIndent) {
        blockForIndent = CreateIndentBlock(blockQName);
        InsertNodeBefore(blockForIndent, firstSibling, editingState);
    }

    MoveRemainingSiblingsToNewParent(firstSibling, lastSibling->nextSibling(), blockForIndent, editingState);
    if (needToMergeNextSibling) {
        MoveRemainingSiblingsToNewParent(nextSibling->firstChild(), nextSibling->lastChild()->nextSibling(), blockForIndent, editingState);
        RemoveNode(nextSibling, editingState);
    }
}

}
