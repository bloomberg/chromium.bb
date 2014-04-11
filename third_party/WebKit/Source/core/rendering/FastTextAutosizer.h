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
#include "core/rendering/RenderTable.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/Noncopyable.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"

namespace WebCore {

class Document;
class RenderBlock;
class RenderListItem;
class RenderListMarker;

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

    void updatePageInfoInAllFrames();
    void updatePageInfo();
    void record(const RenderBlock*);
    void destroy(const RenderBlock*);
    void inflateListItem(RenderListItem*, RenderListMarker*);

    class LayoutScope {
    public:
        explicit LayoutScope(RenderBlock*);
        ~LayoutScope();
    private:
        FastTextAutosizer* m_textAutosizer;
        RenderBlock* m_block;
    };

    class DeferUpdatePageInfo {
    public:
        explicit DeferUpdatePageInfo(Page*);
        ~DeferUpdatePageInfo();
    private:
        RefPtr<LocalFrame> m_mainFrame;
    };

private:
    typedef HashSet<const RenderBlock*> BlockSet;

    enum HasEnoughTextToAutosize {
        UnknownAmountOfText,
        HasEnoughText,
        NotEnoughText
    };

    enum RelayoutBehavior {
        AlreadyInLayout, // The default; appropriate if we are already in layout.
        LayoutNeeded // Use this if changing a multiplier outside of layout.
    };

    enum BeginLayoutBehavior {
        StopLayout,
        ContinueLayout
    };

    // A supercluster represents autosizing information about a set of two or
    // more blocks that all have the same fingerprint. Clusters whose roots
    // belong to a supercluster will share a common multiplier and
    // text-length-based autosizing status.
    struct Supercluster {
        explicit Supercluster(const BlockSet* roots)
            : m_roots(roots)
            , m_multiplier(0)
        {
        }

        const BlockSet* const m_roots;
        float m_multiplier;
    };

    struct Cluster {
        explicit Cluster(const RenderBlock* root, bool autosize, Cluster* parent, Supercluster* supercluster = 0)
            : m_root(root)
            , m_deepestBlockContainingAllText(0)
            , m_parent(parent)
            , m_autosize(autosize)
            , m_multiplier(0)
            , m_hasEnoughTextToAutosize(UnknownAmountOfText)
            , m_supercluster(supercluster)
            , m_hasTableAncestor(root->isTableCell() || (m_parent && m_parent->m_hasTableAncestor))
        {
        }

        const RenderBlock* const m_root;
        // The deepest block containing all text is computed lazily (see:
        // deepestBlockContainingAllText). A value of 0 indicates the value has not been computed yet.
        const RenderBlock* m_deepestBlockContainingAllText;
        Cluster* m_parent;
        bool m_autosize;
        // The multiplier is computed lazily (see: clusterMultiplier) because it must be calculated
        // after the lowest block containing all text has entered layout (the
        // m_blocksThatHaveBegunLayout assertions cover this). Note: the multiplier is still
        // calculated when m_autosize is false because child clusters may depend on this multiplier.
        float m_multiplier;
        HasEnoughTextToAutosize m_hasEnoughTextToAutosize;
        // A set of blocks that are similar to this block.
        Supercluster* m_supercluster;
        bool m_hasTableAncestor;
    };

    enum TextLeafSearch {
        First,
        Last
    };

    struct FingerprintSourceData {
        FingerprintSourceData()
            : m_parentHash(0)
            , m_qualifiedNameHash(0)
            , m_packedStyleProperties(0)
            , m_column(0)
            , m_width(0)
        {
        }

        unsigned m_parentHash;
        unsigned m_qualifiedNameHash;
        // Style specific selection of signals
        unsigned m_packedStyleProperties;
        unsigned m_column;
        float m_width;
    };
    // Ensures efficient hashing using StringHasher.
    COMPILE_ASSERT(!(sizeof(FingerprintSourceData) % sizeof(UChar)),
        Sizeof_FingerprintSourceData_must_be_multiple_of_UChar);

    typedef unsigned Fingerprint;
    typedef HashMap<Fingerprint, OwnPtr<Supercluster> > SuperclusterMap;
    typedef Vector<OwnPtr<Cluster> > ClusterStack;

    // Fingerprints are computed during style recalc, for (some subset of)
    // blocks that will become cluster roots.
    class FingerprintMapper {
    public:
        void add(const RenderObject*, Fingerprint);
        void addTentativeClusterRoot(const RenderBlock*, Fingerprint);
        void remove(const RenderObject*);
        Fingerprint get(const RenderObject*);
        BlockSet& getTentativeClusterRoots(Fingerprint);
    private:
        typedef HashMap<const RenderObject*, Fingerprint> FingerprintMap;
        typedef HashMap<Fingerprint, OwnPtr<BlockSet> > ReverseFingerprintMap;

        FingerprintMap m_fingerprints;
        ReverseFingerprintMap m_blocksForFingerprint;
#ifndef NDEBUG
        void assertMapsAreConsistent();
#endif
    };

    explicit FastTextAutosizer(const Document*);

    void beginLayout(RenderBlock*);
    void endLayout(RenderBlock*);
    void inflateTable(RenderTable*);
    void inflate(RenderBlock*);
    bool enabled() const;
    bool shouldHandleLayout() const;
    void setAllTextNeedsLayout();
    void resetMultipliers();
    BeginLayoutBehavior prepareForLayout(const RenderBlock*);
    void prepareClusterStack(const RenderObject*);
    bool isFingerprintingCandidate(const RenderBlock*);
    bool clusterHasEnoughTextToAutosize(Cluster*, const RenderBlock* widthProvider = 0);
    bool anyClusterHasEnoughTextToAutosize(const BlockSet* roots, const RenderBlock* widthProvider = 0);
    bool clusterWouldHaveEnoughTextToAutosize(const RenderBlock* root, const RenderBlock* widthProvider = 0);
    Fingerprint getFingerprint(const RenderObject*);
    Fingerprint computeFingerprint(const RenderObject*);
    Cluster* maybeCreateCluster(const RenderBlock*);
    Supercluster* getSupercluster(const RenderBlock*);
    float clusterMultiplier(Cluster*);
    float superclusterMultiplier(Cluster*);
    // A cluster's width provider is typically the deepest block containing all text.
    // There are exceptions, such as tables and table cells which use the table itself for width.
    const RenderBlock* clusterWidthProvider(const RenderBlock*);
    const RenderBlock* maxClusterWidthProvider(const Supercluster*, const RenderBlock* currentRoot);
    // Typically this returns a block's computed width. In the case of tables layout, this
    // width is not yet known so the fixed width is used if it's available, or the containing
    // block's width otherwise.
    float widthFromBlock(const RenderBlock*);
    float multiplierFromBlock(const RenderBlock*);
    void applyMultiplier(RenderObject*, float, RelayoutBehavior = AlreadyInLayout);
    bool isWiderOrNarrowerDescendant(Cluster*);
    bool isLayoutRoot(const RenderBlock*) const;

    Cluster* currentCluster() const;

    RenderObject* nextChildSkippingChildrenOfBlocks(const RenderObject*, const RenderObject*);

    const RenderBlock* deepestBlockContainingAllText(Cluster*);
    const RenderBlock* deepestBlockContainingAllText(const RenderBlock*);
    // Returns the first text leaf that is in the current cluster. We attempt to not include text
    // from descendant clusters but because descendant clusters may not exist, this is only an approximation.
    // The TraversalDirection controls whether we return the first or the last text leaf.
    const RenderObject* findTextLeaf(const RenderObject*, size_t&, TextLeafSearch);

    const Document* m_document;
    int m_frameWidth; // LocalFrame width in density-independent pixels (DIPs).
    int m_layoutWidth; // Layout width in CSS pixels.
    float m_baseMultiplier; // Includes accessibility font scale factor and device scale adjustment.
    bool m_pageNeedsAutosizing;
    bool m_previouslyAutosized;
    bool m_updatePageInfoDeferred;
    const RenderBlock* m_firstBlockToBeginLayout;
#ifndef NDEBUG
    BlockSet m_blocksThatHaveBegunLayout; // Used to ensure we don't compute properties of a block before beginLayout() is called on it.
#endif

    // Clusters are created and destroyed during layout. The map key is the
    // cluster root. Clusters whose roots share the same fingerprint use the
    // same multiplier.
    SuperclusterMap m_superclusters;
    ClusterStack m_clusterStack;
    FingerprintMapper m_fingerprintMapper;
    Vector<RefPtr<RenderStyle> > m_stylesRetainedDuringLayout;
};

} // namespace WebCore

#endif // FastTextAutosizer_h
