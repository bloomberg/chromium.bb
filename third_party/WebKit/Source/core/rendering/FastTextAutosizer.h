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
    static PassOwnPtr<FastTextAutosizer> create(const Document* document)
    {
        return adoptPtr(new FastTextAutosizer(document));
    }

    void record(const RenderBlock*);
    void destroy(const RenderBlock*);

    void beginLayout(RenderBlock*);
    void endLayout(RenderBlock*);

private:
    struct Cluster {
        explicit Cluster(const RenderBlock* root, bool autosize, float multiplier)
            : m_root(root)
            , m_autosize(autosize)
            , m_multiplier(multiplier)
        {
        }
        const RenderBlock* m_root;
        const bool m_autosize;
        float m_multiplier;
    };

    typedef HashSet<const RenderBlock*> BlockSet;
    typedef HashMap<const RenderBlock*, OwnPtr<Cluster> > ClusterMap;
    typedef Vector<Cluster*> ClusterStack;

    // Fingerprints are computed during style recalc, for (some subset of)
    // blocks that will become cluster roots.
    class FingerprintMapper {
    public:
        void add(const RenderBlock*, AtomicString);
        void remove(const RenderBlock*);
        AtomicString get(const RenderBlock*);
        BlockSet& getBlocks(AtomicString);
    private:
        typedef HashMap<const RenderBlock*, AtomicString> FingerprintMap;
        typedef HashMap<AtomicString, OwnPtr<BlockSet> > ReverseFingerprintMap;

        FingerprintMap m_fingerprints;
        ReverseFingerprintMap m_blocksForFingerprint;
    };

    explicit FastTextAutosizer(const Document*);

    void inflate(RenderBlock*);
    bool enabled();
    void prepareRenderViewInfo(RenderView*);
    bool isFingerprintingCandidate(const RenderBlock*);
    bool clusterWantsAutosizing(const RenderBlock*);
    AtomicString computeFingerprint(const RenderBlock*);
    Cluster* maybeGetOrCreateCluster(const RenderBlock*);
    Cluster* addSupercluster(AtomicString, const RenderBlock*);
    const RenderBlock* deepestCommonAncestor(BlockSet&);
    float computeMultiplier(const RenderBlock*);
    void applyMultiplier(RenderObject*, float);

    RenderObject* nextChildSkippingChildrenOfBlocks(const RenderObject*, const RenderObject*);

    const Document* m_document;
    int m_frameWidth; // Frame width in density-independent pixels (DIPs).
    int m_layoutWidth; // Layout width in CSS pixels.
    float m_baseMultiplier; // Includes accessibility font scale factor and device scale adjustment.
#ifndef NDEBUG
    bool m_renderViewInfoPrepared; // Used for assertions.
#endif

    // Clusters are created and destroyed during layout. The map key is the
    // cluster root. Clusters whose roots share the same fingerprint use the
    // same multiplier.
    ClusterMap m_clusters;
    ClusterStack m_clusterStack;
    FingerprintMapper m_fingerprintMapper;
};

} // namespace WebCore

#endif // FastTextAutosizer_h
