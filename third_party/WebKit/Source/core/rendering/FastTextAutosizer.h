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
#include "core/rendering/TextAutosizer.h"
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
        explicit Cluster(const RenderBlock* root, bool autosize, Cluster* parent)
            : m_root(root)
            , m_parent(parent)
            , m_autosize(autosize)
            , m_multiplier(0)
        {
        }

        const RenderBlock* m_root;
        Cluster* m_parent;
        bool m_autosize;
        // The multiplier is computed lazily (see: clusterMultiplier) because it must be calculated
        // after the lowest block containing all text has entered layout (the
        // m_blocksThatHaveBegunLayout assertions cover this). Note: the multiplier is still
        // calculated when m_autosize is false because child clusters may depend on this multiplier.
        float m_multiplier;
    };

    enum TextLeafSearch {
        First,
        Last
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
    float clusterMultiplier(Cluster*);
    void applyMultiplier(RenderObject*, float);

    Cluster* currentCluster() const;

    RenderObject* nextChildSkippingChildrenOfBlocks(const RenderObject*, const RenderObject*);
    const RenderBlock* findDeepestBlockContainingAllText(const RenderBlock*);
    // Returns the first text leaf that is in the current cluster and not in a descendent cluster.
    // The TraversalDirection controls whether we return the first or the last text leaf.
    const RenderObject* findTextLeaf(const RenderObject*, size_t&, TextLeafSearch);

    const Document* m_document;
    int m_frameWidth; // Frame width in density-independent pixels (DIPs).
    int m_layoutWidth; // Layout width in CSS pixels.
    float m_baseMultiplier; // Includes accessibility font scale factor and device scale adjustment.
#ifndef NDEBUG
    bool m_renderViewInfoPrepared;
    BlockSet m_blocksThatHaveBegunLayout; // Used to ensure we don't compute properties of a block before beginLayout() is called on it.
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
