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

#include "wtf/Vector.h"

namespace WebCore {

class Frame;
class Document;
class Frame;
class HTMLImportsController;

class HTMLImport {
public:
    static bool unblock(HTMLImport*);

    virtual ~HTMLImport() { }

    Frame* frame();
    Document* master();

    bool isLoaded() const { return !isBlocked() && !isProcessing(); }
    bool isBlocked() const { return m_blocked; }
    void appendChild(HTMLImport*);

    virtual HTMLImportsController* controller() = 0;
    virtual HTMLImport* parent() const = 0;
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
    bool hasChildren() const { return !m_children.isEmpty(); }

    Vector<HTMLImport*> m_children;
    bool m_blocked; // If any of decendants or predecessors is in processing, it is blocked.
};

} // namespace WebCore

#endif // HTMLImport_h
