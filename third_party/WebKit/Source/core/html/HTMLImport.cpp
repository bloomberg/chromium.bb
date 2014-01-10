/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#include "config.h"
#include "core/html/HTMLImport.h"

#include "core/dom/Document.h"

namespace WebCore {

Frame* HTMLImport::frame()
{
    return master()->frame();
}

Document* HTMLImport::master()
{
    return root()->document();
}

HTMLImportsController* HTMLImport::controller()
{
    return root()->toController();
}

void HTMLImport::appendChild(HTMLImport* child)
{
    if (isBlockedFromRunningScript())
        child->blockFromRunningScript();
    if (lastChild() && lastChild()->isBlockingFollowersFromCreatingDocument())
        child->blockFromCreatingDocument();
    TreeNode<HTMLImport>::appendChild(child);
    if (child->isCreatedByParser())
        blockPredecessorsOf(child);
}

inline bool HTMLImport::isBlockedFromCreatingDocumentByPredecessors() const
{
    ASSERT(isBlockedFromCreatingDocument());
    HTMLImport* elder = previousSibling();
    return (elder && !elder->isDone());
}

bool HTMLImport::isBlockedFromRunningScriptByPredecessors() const
{
    HTMLImport* parent = this->parent();
    if (!parent)
        return false;

    for (HTMLImport* sibling = parent->firstChild(); sibling; sibling = sibling->nextSibling()) {
        if (sibling == this)
            break;
        if (sibling->isBlockingFollowersFromRunningScript())
            return true;
    }

    return false;
}

void HTMLImport::waitLoaderOrChildren()
{
    if (WaitingLoaderOrChildren < m_state)
        m_state = WaitingLoaderOrChildren;
}

void HTMLImport::blockFromRunningScript()
{
    if (BlockedFromRunningScript < m_state)
        m_state = BlockedFromRunningScript;
}

void HTMLImport::blockFromCreatingDocument()
{
    if (BlockedFromCreatingDocument < m_state)
        m_state = BlockedFromCreatingDocument;
}

void HTMLImport::becomeReady()
{
    if (!isBlocked())
        return;
    m_state = Ready;
    didBecomeReady();
}

void HTMLImport::unblockFromRunningScript()
{
    if (!isBlockedFromRunningScript())
        return;
    m_state = WaitingLoaderOrChildren;
    didUnblockFromRunningScript();
}

void HTMLImport::unblockFromCreatingDocument()
{
    if (!isBlockedFromCreatingDocument())
        return;
    m_state = BlockedFromRunningScript;
    didUnblockFromCreatingDocument();
}

void HTMLImport::didUnblockFromRunningScript()
{
    ASSERT(!isBlockedFromCreatingDocument());
    ASSERT(!isBlockedFromRunningScript());
    if (Document* document = this->document())
        document->didLoadAllImports();
}

void HTMLImport::didBecomeReady()
{
    ASSERT(isDone());
}

void HTMLImport::didUnblockFromCreatingDocument()
{
    ASSERT(!isBlockedFromCreatingDocument());
}

void HTMLImport::loaderWasResolved()
{
    unblockFromCreatingDocument();
    unblockFromRunningScript();
}

void HTMLImport::loaderDidFinish()
{
    if (m_state == WaitingLoaderOrChildren)
        becomeReady();
    root()->blockerGone();
}

inline bool HTMLImport::isBlockingFollowersFromRunningScript() const
{
    if (!isCreatedByParser())
        return false;
    if (isBlockedFromRunningScript())
        return true;
    // Blocking here can result dead lock if the node doesn't own loader and has shared loader.
    // Because the shared loader can point its ascendant and forms a cycle.
    if (!ownsLoader())
        return false;
    return !isDone();
}

inline bool HTMLImport::isBlockingFollowersFromCreatingDocument() const
{
    return !isDone();
}

bool HTMLImport::unblock(HTMLImport* import)
{
    ASSERT(!import->isBlockedFromRunningScriptByPredecessors());

    if (import->isBlockedFromCreatingDocument() && import->isBlockedFromCreatingDocumentByPredecessors())
        return false;
    import->unblockFromCreatingDocument();

    for (HTMLImport* child = import->firstChild(); child; child = child->nextSibling()) {
        if (!unblock(child))
            return false;
    }

    import->unblockFromRunningScript();
    if (import->isDone())
        import->becomeReady();

    return !import->isBlockingFollowersFromRunningScript();
}

void HTMLImport::block(HTMLImport* import)
{
    for (HTMLImport* child = import; child; child = traverseNext(*child, import))
        child->blockFromRunningScript();
}

void HTMLImport::blockPredecessorsOf(HTMLImport* child)
{
    ASSERT(child->parent() == this);

    for (HTMLImport* sibling = lastChild(); sibling; sibling = sibling->previousSibling()) {
        if (sibling == child)
            break;
        HTMLImport::block(sibling);
    }

    this->blockFromRunningScript();

    if (HTMLImport* parent = this->parent()) {
        if (isCreatedByParser())
            parent->blockPredecessorsOf(this);
    }
}

bool HTMLImport::isMaster(Document* document)
{
    if (!document->import())
        return true;
    return (document->import()->master() == document);
}

} // namespace WebCore
