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

#ifndef FastTextAutosizer_h
#define FastTextAutosizer_h

#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/Noncopyable.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/AtomicStringHash.h"

namespace WebCore {

class Document;
class RenderBlock;

// Single-pass text autosizer (work in progress). Works in two stages:
// (1) record information about page elements during style recalc
// (2) inflate sizes during layout
// See: http://tinyurl.com/chromium-fast-autosizer

class FastTextAutosizer FINAL {
    WTF_MAKE_NONCOPYABLE(FastTextAutosizer);

public:
    static PassOwnPtr<FastTextAutosizer> create(Document* document)
    {
        return adoptPtr(new FastTextAutosizer(document));
    }

    void record(const RenderBlock*);
    void destroy(const RenderBlock*);
    void inflate(RenderBlock*);

private:
    // TODO: make a proper API for this class?
    struct Cluster {
        explicit Cluster(AtomicString fingerprint)
            : m_fingerprint(fingerprint)
            , m_multiplier(0)
        {
        }

        AtomicString m_fingerprint;
        WTF::HashSet<const RenderBlock*> m_blocks;
        float m_multiplier;
    };

    explicit FastTextAutosizer(Document*);

    AtomicString fingerprint(const RenderBlock*);
    float computeMultiplier(const Cluster*);

    Document* m_document;

    WTF::HashMap<const RenderBlock*, Cluster*> m_clusterForBlock;
    WTF::HashMap<AtomicString, OwnPtr<Cluster> > m_clusterForFingerprint;
};

} // namespace WebCore

#endif // FastTextAutosizer_h
