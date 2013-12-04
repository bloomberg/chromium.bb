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
class HTMLImportRoot;
class HTMLImportsController;

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
// * The non-root nodes are HTMLImportLoader (FIXME: rename to HTMLImportChild),
//   which is owned by LinkStyle, that is owned by HTMLLinkElement.
//   LinkStyle is wired into HTMLImportLoader by implementing HTMLImportLoaderClient interface
//
// * Both HTMLImportsController and HTMLImportLoader are derived from HTMLImport superclass
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
//               <|-                    HTMLImportLoader <- LinkStyle <- HTMLLinkElement
//
//
// # Import Sharing and HTMLImportData
//
// The HTML Imports spec calls for de-dup mechanism to share already loaded imports.
// To implement this, the actual loading machinery is split out from HTMLImportLoader to
// HTMLImportData (FIXME: rename HTMLImportLoader),
// and each loader shares HTMLImportData with other loader if the URL is same.
// Check around HTMLImportsController::findLink() for more detail.
//
// Note that HTMLImportData provides HTMLImportDataClient to hook it up.
// As it can be shared, HTMLImportData supports multiple clients.
//
//    HTMLImportLoader (1)-->(*) HTMLImportData
//
//
// # Script Blocking
//
// The HTML parser blocks on <script> when preceding <link>s aren't finish loading imports.
// Each HTMLImport instance tracks such a blocking state, that is HTMLImport::isBlocked().
//
// ## Blocking Imports
//
// Each imports can become being blocked when new imports are added to the import tree.
// For example, the parser of a parent import is blocked when new child import is given.
// See HTMLImport::appendChild() to see how it is handled. Note that this blocking-ness is
// transitive. HTMLImport::blockAfter() flips the flags iteratively to fullfill this transitivity.
//
// ## Unblocking Imports
//
// The state can change when one of the imports finish loading. The Document notices it through
// HTMLImportRoot::blockerGone(). The blockerGone() triggers HTMLImport::unblock(), which traverses
// whole import tree and find unblock-able imports and unblock them.
// Unblocked imported documents are notified through Document::didLoadAllImports() so that
// it can resume its parser.
//

// The superclass of HTMLImportsController and HTMLImportLoader
// This represents the import tree data structure.
class HTMLImport : public TreeNode<HTMLImport> {
public:
    static bool unblock(HTMLImport*);
    static bool isMaster(Document*);

    virtual ~HTMLImport() { }

    Frame* frame();
    Document* master();
    HTMLImportsController* controller();

    bool isLoaded() const { return !isBlocked() && !isProcessing(); }
    bool isBlocked() const { return m_blocked; }
    void appendChild(HTMLImport*);

    virtual HTMLImportRoot* root() = 0;
    virtual Document* document() const = 0;
    virtual void wasDetachedFromDocument() = 0;
    virtual void didFinishParsing() = 0;
    virtual bool isProcessing() const = 0;

protected:
    HTMLImport()
        : m_blocked(false)
    { }

private:
    static void block(HTMLImport*);

    void blockAfter(HTMLImport* child);
    void block();
    void unblock();
    void didUnblock();

    bool arePredecessorsLoaded() const;
    bool areChilrenLoaded() const;

    bool m_blocked; // If any of decendants or predecessors is in processing, it is blocked.
};

// An abstract class to decouple its sublcass HTMLImportsController.
class HTMLImportRoot : public HTMLImport {
public:
    virtual void blockerGone() = 0;
    virtual HTMLImportsController* toController() = 0;
};

} // namespace WebCore

#endif // HTMLImport_h
