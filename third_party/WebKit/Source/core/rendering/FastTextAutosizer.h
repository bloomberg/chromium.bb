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

#include "core/rendering/RenderObject.h"
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

    void prepareForLayout();

    void record(RenderBlock*);
    void destroy(RenderBlock*);
    void inflate(RenderBlock*);

private:
    // FIXME(crbug.com/327811): make a proper API for this class.
    struct Cluster {
        explicit Cluster(AtomicString fingerprint)
            : m_fingerprint(fingerprint)
            , m_multiplier(0)
        {
        }

        AtomicString m_fingerprint;
        WTF::HashSet<RenderBlock*> m_blocks;
        float m_multiplier;
        const RenderObject* m_clusterRoot;

        void addBlock(RenderBlock* block)
        {
            m_blocks.add(block);
            setNeedsClusterRecalc();
        }

        void setNeedsClusterRecalc() { m_multiplier = 0; m_clusterRoot = 0; }
        bool needsClusterRecalc() const { return !m_clusterRoot || m_multiplier <= 0; }
    };

    explicit FastTextAutosizer(Document*);

    bool updateWindowWidth();

    AtomicString fingerprint(const RenderBlock*);
    void recalcClusterIfNeeded(Cluster*);

    int m_windowWidth;

    Document* m_document;

    typedef WTF::HashMap<const RenderBlock*, Cluster*> BlockToClusterMap;
    typedef WTF::HashMap<AtomicString, OwnPtr<Cluster> > FingerprintToClusterMap;
    BlockToClusterMap m_clusterForBlock;
    FingerprintToClusterMap m_clusterForFingerprint;
};

} // namespace WebCore

#endif // FastTextAutosizer_h
