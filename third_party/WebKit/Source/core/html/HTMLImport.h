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

#ifndef HTMLImport_h
#define HTMLImport_h

#include "wtf/TreeNode.h"
#include "wtf/Vector.h"

namespace WebCore {

class CustomElementMicrotaskImportStep;
class Document;
class Frame;
class HTMLImportChild;
class HTMLImportRoot;
class HTMLImportsController;
class KURL;

//
// # Basic Data Structure and Algorithms of HTML Imports implemenation.
//
// ## The Import Tree
//
// HTML Imports form a tree:
//
// * The root of the tree is HTMLImportsController, which is owned by the master
//   document as a DocumentSupplement. HTMLImportsController has an abstract class called
//   HTMLImportRoot to deal with cycler dependency.
//
// * The non-root nodes are HTMLImportChild, which is owned by LinkStyle, that is owned by HTMLLinkElement.
//   LinkStyle is wired into HTMLImportChild by implementing HTMLImportChildClient interface
//
// * Both HTMLImportsController and HTMLImportChild are derived from HTMLImport superclass
//   that models the tree data structure using WTF::TreeNode and provides a set of
//   virtual functions.
//
// HTMLImportsController also owns all loaders in the tree and manages their lifetime through it.
// One assumption is that the tree is append-only and nodes are never inserted in the middle of the tree nor removed.
//
//
//    HTMLImport <|- HTMLImportRoot <|- HTMLImportsController <- Document
//                                      *
//                                      |
//               <|-                    HTMLImportChild <- LinkStyle <- HTMLLinkElement
//
//
// # Import Sharing and HTMLImportLoader
//
// The HTML Imports spec calls for de-dup mechanism to share already loaded imports.
// To implement this, the actual loading machinery is split out from HTMLImportChild to
// HTMLImportLoader, and each loader shares HTMLImportLoader with other loader if the URL is same.
// Check around HTMLImportsController::findLink() for more detail.
//
// Note that HTMLImportLoader provides HTMLImportLoaderClient to hook it up.
// As it can be shared, HTMLImportLoader supports multiple clients.
//
//    HTMLImportChild (1)-->(*) HTMLImportLoader
//
//
// # Script Blocking
//
// The HTML parser blocks on <script> when preceding <link>s aren't finish loading imports.
// Each HTMLImport instance tracks such a blocking state, that is called "script-blocked"
// or HTMLImport::isBlockedFromRunningScript().
//
// ## Blocking Imports
//
// Each imports can become being script-blocked when new imports are added to the import tree.
// For example, the parser of a parent import is blocked when new child import is given.
// See HTMLImport::appendChild() to see how it is handled. Note that this blocking-ness is
// transitive. HTMLImport::blockPredecessorsOf() flips the flags iteratively to fullfill this transitivity.
//
// ## Unblocking Imports
//
// The blocking state can change when one of the imports finish loading. The Document notices it through
// HTMLImportRoot::blockerGone(). The blockerGone() triggers HTMLImport::unblockFromRunningScript(), which traverses
// whole import tree and find unblock-able imports and unblock them.
// Ready imported documents are notified through Document::didLoadAllImports() so that
// it can resume its parser.
//
// # Document Blocking
//
// There is another type of blocking state that is called
// "document-blocked". If the import is document-blocked, it cannot
// create its own document object because sharable imported document
// can appear later. The spec defines the loading order of the
// import: The earlier one in the import-tree order should win and
// later ones should share earlier one.
//
// The "document-blocked" state keeps the tree node from loading its
// import until all preceding imports are ready t respect the
// order. Note that the node may fetch the bytes from the URL
// speculatively, even though it doesn't process it.
//
// The node is "document-unblocked" when there are unfinished,
// preceeding import loading. Unblocking attempt for
// "document-blocked" happens at the same timing as unblocking
// "script-blocked".
//

// The superclass of HTMLImportsController and HTMLImportChild
// This represents the import tree data structure.
class HTMLImport : public TreeNode<HTMLImport> {
public:
    static bool unblock(HTMLImport*);
    static bool isMaster(Document*);

    virtual ~HTMLImport() { }

    Frame* frame();
    Document* master();
    HTMLImportsController* controller();
    bool isRoot() const { return !isChild(); }

    bool isCreatedByParser() const { return m_createdByParser; }
    bool isBlockedFromRunningScript() const { return m_state <= BlockedFromRunningScript; }
    bool isBlocked() const { return m_state < Ready; }

    bool isBlockedFromCreatingDocument() const;

    void appendChild(HTMLImport*);

    virtual bool isChild() const { return false; }
    virtual HTMLImportRoot* root() = 0;
    virtual Document* document() const = 0;
    virtual void wasDetachedFromDocument() = 0;
    virtual void didFinishParsing() = 0;
    virtual bool isDone() const = 0; // FIXME: Should be renamed to haveFinishedLoading()
    virtual bool hasLoader() const = 0;
    virtual bool ownsLoader() const { return false; }
    virtual CustomElementMicrotaskImportStep* customElementMicrotaskStep() const { return 0; }

protected:
    enum State {
        BlockedFromRunningScript,
        WaitingLoaderOrChildren,
        Ready
    };

    explicit HTMLImport(State state, bool createdByParser = false)
        : m_state(state)
        , m_createdByParser(createdByParser)
    { }

    virtual void didUnblockFromCreatingDocument();
    virtual void didUnblockFromRunningScript();
    virtual void didBecomeReady();

    void loaderWasResolved();
    void loaderDidFinish();

private:
    static void block(HTMLImport*);

    void blockPredecessorsOf(HTMLImport* child);
    void blockFromRunningScript();
    void waitLoaderOrChildren();
    void unblockFromRunningScript();
    void becomeReady();

    bool isBlockedFromRunningScriptByPredecessors() const;
    bool isBlockingFollowersFromRunningScript() const;
    bool isBlockingFollowersFromCreatingDocument() const;

    State m_state;

    bool m_createdByParser : 1;
};

// An abstract class to decouple its sublcass HTMLImportsController.
class HTMLImportRoot : public HTMLImport {
public:
    HTMLImportRoot() : HTMLImport(Ready) { }

    virtual void blockerGone() = 0;
    virtual HTMLImportsController* toController() = 0;
    virtual HTMLImportChild* findLinkFor(const KURL&, HTMLImport* excluding = 0) const = 0;
};

} // namespace WebCore

#endif // HTMLImport_h
