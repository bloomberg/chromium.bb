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

class Frame;
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
// or HTMLImport::isScriptBlocked().
//
// ## Blocking Imports
//
// Each imports can become being script-blocked when new imports are added to the import tree.
// For example, the parser of a parent import is blocked when new child import is given.
// See HTMLImport::appendChild() to see how it is handled. Note that this blocking-ness is
// transitive. HTMLImport::blockAfter() flips the flags iteratively to fullfill this transitivity.
//
// ## Unblocking Imports
//
// The blocking state can change when one of the imports finish loading. The Document notices it through
// HTMLImportRoot::blockerGone(). The blockerGone() triggers HTMLImport::unblockScript(), which traverses
// whole import tree and find unblock-able imports and unblock them.
// Unblocked imported documents are notified through Document::didLoadAllImports() so that
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

    bool isLoaded() const { return !isScriptBlocked() && !isProcessing(); }
    bool isScriptBlocked() const { return m_scriptBlocked; }
    bool isDocumentBlocked() const { return m_documentBlocked; }
    bool isBlocked() const { return m_scriptBlocked || m_documentBlocked; }

    void appendChild(HTMLImport*);

    virtual HTMLImportRoot* root() = 0;
    virtual Document* document() const = 0;
    virtual void wasDetachedFromDocument() = 0;
    virtual void didFinishParsing() = 0;
    virtual bool isProcessing() const = 0;
    virtual bool isDone() const = 0;

protected:
    HTMLImport()
        : m_scriptBlocked(false)
        , m_documentBlocked(false)
    { }

    virtual void didUnblockDocument();

private:
    static void block(HTMLImport*);

    void blockAfter(HTMLImport* child);
    void blockScript();
    void unblockScript();
    void didUnblockScript();

    void blockDocument();
    void unblockDocument();

    bool needsBlockingDocument() const;
    bool arePredecessorsLoaded() const;
    bool areChilrenLoaded() const;

    bool m_scriptBlocked; // If any of decendants or predecessors is in processing, the parser blocks on <script>.
    bool m_documentBlocked; // If its predecessor is not done yet, the document creation is blocked.
};

// An abstract class to decouple its sublcass HTMLImportsController.
class HTMLImportRoot : public HTMLImport {
public:
    virtual void blockerGone() = 0;
    virtual HTMLImportsController* toController() = 0;
    virtual HTMLImportChild* findLinkFor(const KURL&, HTMLImport* excluding = 0) const = 0;
};

} // namespace WebCore

#endif // HTMLImport_h
