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

#ifndef TextAutosizer_h
#define TextAutosizer_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/Noncopyable.h"
#include <memory>
#include <unicode/uchar.h>

namespace blink {

class ComputedStyle;
class Document;
class IntSize;
class LayoutBlock;
class LayoutObject;
class LayoutTable;
class LayoutText;
class LocalFrame;
class Page;
class SubtreeLayoutScope;

// Single-pass text autosizer. Documentation at:
// http://tinyurl.com/TextAutosizer

class CORE_EXPORT TextAutosizer final
    : public GarbageCollectedFinalized<TextAutosizer> {
  WTF_MAKE_NONCOPYABLE(TextAutosizer);

 public:
  ~TextAutosizer();
  static TextAutosizer* create(const Document* document) {
    return new TextAutosizer(document);
  }
  static float computeAutosizedFontSize(float specifiedSize, float multiplier);

  void updatePageInfoInAllFrames();
  void updatePageInfo();
  void record(LayoutBlock*);
  void record(LayoutText*);
  void destroy(LayoutBlock*);

  bool pageNeedsAutosizing() const;

  DECLARE_TRACE();

  class LayoutScope {
    STACK_ALLOCATED();

   public:
    explicit LayoutScope(LayoutBlock*, SubtreeLayoutScope* = nullptr);
    ~LayoutScope();

   protected:
    Member<TextAutosizer> m_textAutosizer;
    LayoutBlock* m_block;
  };

  class TableLayoutScope : LayoutScope {
    STACK_ALLOCATED();

   public:
    explicit TableLayoutScope(LayoutTable*);
  };

  class CORE_EXPORT DeferUpdatePageInfo {
    STACK_ALLOCATED();

   public:
    explicit DeferUpdatePageInfo(Page*);
    ~DeferUpdatePageInfo();

   private:
    Member<LocalFrame> m_mainFrame;
  };

 private:
  typedef HashSet<LayoutBlock*> BlockSet;
  typedef HashSet<const LayoutBlock*> ConstBlockSet;

  enum HasEnoughTextToAutosize {
    UnknownAmountOfText,
    HasEnoughText,
    NotEnoughText
  };

  enum RelayoutBehavior {
    AlreadyInLayout,  // The default; appropriate if we are already in layout.
    LayoutNeeded      // Use this if changing a multiplier outside of layout.
  };

  enum BeginLayoutBehavior { StopLayout, ContinueLayout };

  enum InflateBehavior { ThisBlockOnly, DescendToInnerBlocks };

  enum BlockFlag {
    // A block that is evaluated for becoming a cluster root.
    POTENTIAL_ROOT = 1 << 0,
    // A cluster root that establishes an independent multiplier.
    INDEPENDENT = 1 << 1,
    // A cluster root with an explicit width. These are likely to be
    // independent.
    EXPLICIT_WIDTH = 1 << 2,
    // A cluster that is wider or narrower than its parent. These also create an
    // independent multiplier, but this state cannot be determined until layout.
    WIDER_OR_NARROWER = 1 << 3,
    // A cluster that suppresses autosizing.
    SUPPRESSING = 1 << 4
  };

  enum InheritParentMultiplier {
    Unknown,
    InheritMultiplier,
    DontInheritMultiplier
  };

  typedef unsigned BlockFlags;

  // A supercluster represents autosizing information about a set of two or
  // more blocks that all have the same fingerprint. Clusters whose roots
  // belong to a supercluster will share a common multiplier and
  // text-length-based autosizing status.
  struct Supercluster {
    USING_FAST_MALLOC(Supercluster);

   public:
    explicit Supercluster(const BlockSet* roots)
        : m_roots(roots),
          m_hasEnoughTextToAutosize(UnknownAmountOfText),
          m_multiplier(0),
          m_inheritParentMultiplier(Unknown) {}

    const BlockSet* m_roots;
    HasEnoughTextToAutosize m_hasEnoughTextToAutosize;
    float m_multiplier;
    InheritParentMultiplier m_inheritParentMultiplier;
  };

  struct Cluster {
    USING_FAST_MALLOC(Cluster);

   public:
    explicit Cluster(const LayoutBlock* root,
                     BlockFlags,
                     Cluster* parent,
                     Supercluster* = nullptr);

    const LayoutBlock* const m_root;
    BlockFlags m_flags;
    // The deepest block containing all text is computed lazily (see:
    // deepestBlockContainingAllText). A value of 0 indicates the value has not
    // been computed yet.
    const LayoutBlock* m_deepestBlockContainingAllText;
    Cluster* m_parent;
    // The multiplier is computed lazily (see: clusterMultiplier) because it
    // must be calculated after the lowest block containing all text has entered
    // layout (the m_blocksThatHaveBegunLayout assertions cover this). Note: the
    // multiplier is still calculated when m_autosize is false because child
    // clusters may depend on this multiplier.
    float m_multiplier;
    HasEnoughTextToAutosize m_hasEnoughTextToAutosize;
    // A set of blocks that are similar to this block.
    Supercluster* m_supercluster;
    bool m_hasTableAncestor;
  };

  enum TextLeafSearch { First, Last };

  struct FingerprintSourceData {
    STACK_ALLOCATED();
    FingerprintSourceData()
        : m_parentHash(0),
          m_qualifiedNameHash(0),
          m_packedStyleProperties(0),
          m_column(0),
          m_width(0) {}

    unsigned m_parentHash;
    unsigned m_qualifiedNameHash;
    // Style specific selection of signals
    unsigned m_packedStyleProperties;
    unsigned m_column;
    float m_width;
  };
  // Ensures efficient hashing using StringHasher.
  static_assert(!(sizeof(FingerprintSourceData) % sizeof(UChar)),
                "sizeof(FingerprintSourceData) must be a multiple of UChar");

  typedef unsigned Fingerprint;
  typedef Vector<std::unique_ptr<Cluster>> ClusterStack;

  // Fingerprints are computed during style recalc, for (some subset of)
  // blocks that will become cluster roots.
  // Clusters whose roots share the same fingerprint use the same multiplier
  class FingerprintMapper {
    DISALLOW_NEW();

   public:
    void add(LayoutObject*, Fingerprint);
    void addTentativeClusterRoot(LayoutBlock*, Fingerprint);
    // Returns true if any BlockSet was modified or freed by the removal.
    bool remove(LayoutObject*);
    Fingerprint get(const LayoutObject*);
    BlockSet* getTentativeClusterRoots(Fingerprint);
    Supercluster* createSuperclusterIfNeeded(LayoutBlock*, bool& isNewEntry);
    bool hasFingerprints() const { return !m_fingerprints.isEmpty(); }
    HashSet<Supercluster*>& getPotentiallyInconsistentSuperclusters() {
      return m_potentiallyInconsistentSuperclusters;
    }

   private:
    typedef HashMap<const LayoutObject*, Fingerprint> FingerprintMap;
    typedef HashMap<Fingerprint, std::unique_ptr<BlockSet>>
        ReverseFingerprintMap;
    typedef HashMap<Fingerprint, std::unique_ptr<Supercluster>> SuperclusterMap;

    FingerprintMap m_fingerprints;
    ReverseFingerprintMap m_blocksForFingerprint;
    // Maps fingerprints to superclusters. Superclusters persist across layouts.
    SuperclusterMap m_superclusters;
    // Superclusters that need to be checked for consistency at the start of the
    // next layout.
    HashSet<Supercluster*> m_potentiallyInconsistentSuperclusters;
#if DCHECK_IS_ON()
    void assertMapsAreConsistent();
#endif
  };

  struct PageInfo {
    DISALLOW_NEW();
    PageInfo()
        : m_frameWidth(0),
          m_layoutWidth(0),
          m_accessibilityFontScaleFactor(1),
          m_deviceScaleAdjustment(1),
          m_pageNeedsAutosizing(false),
          m_hasAutosized(false),
          m_settingEnabled(false) {}

    int m_frameWidth;  // LocalFrame width in density-independent pixels (DIPs).
    int m_layoutWidth;  // Layout width in CSS pixels.
    float m_accessibilityFontScaleFactor;
    float m_deviceScaleAdjustment;
    bool m_pageNeedsAutosizing;
    bool m_hasAutosized;
    bool m_settingEnabled;
  };

  explicit TextAutosizer(const Document*);

  void beginLayout(LayoutBlock*, SubtreeLayoutScope*);
  void endLayout(LayoutBlock*);
  void inflateAutoTable(LayoutTable*);
  float inflate(LayoutObject*,
                SubtreeLayoutScope*,
                InflateBehavior = ThisBlockOnly,
                float multiplier = 0);
  bool shouldHandleLayout() const;
  IntSize windowSize() const;
  void setAllTextNeedsLayout(LayoutBlock* container = nullptr);
  void resetMultipliers();
  BeginLayoutBehavior prepareForLayout(LayoutBlock*);
  void prepareClusterStack(LayoutObject*);
  bool clusterHasEnoughTextToAutosize(
      Cluster*,
      const LayoutBlock* widthProvider = nullptr);
  bool superclusterHasEnoughTextToAutosize(
      Supercluster*,
      const LayoutBlock* widthProvider = nullptr,
      bool skipLayoutedNodes = false);
  bool clusterWouldHaveEnoughTextToAutosize(
      const LayoutBlock* root,
      const LayoutBlock* widthProvider = nullptr);
  Fingerprint getFingerprint(LayoutObject*);
  Fingerprint computeFingerprint(const LayoutObject*);
  Cluster* maybeCreateCluster(LayoutBlock*);
  float clusterMultiplier(Cluster*);
  float superclusterMultiplier(Cluster*);
  // A cluster's width provider is typically the deepest block containing all
  // text. There are exceptions, such as tables and table cells which use the
  // table itself for width.
  const LayoutBlock* clusterWidthProvider(const LayoutBlock*) const;
  const LayoutBlock* maxClusterWidthProvider(
      Supercluster*,
      const LayoutBlock* currentRoot) const;
  // Typically this returns a block's computed width. In the case of tables
  // layout, this width is not yet known so the fixed width is used if it's
  // available, or the containing block's width otherwise.
  float widthFromBlock(const LayoutBlock*) const;
  float multiplierFromBlock(const LayoutBlock*);
  void applyMultiplier(LayoutObject*,
                       float,
                       SubtreeLayoutScope*,
                       RelayoutBehavior = AlreadyInLayout);
  bool isWiderOrNarrowerDescendant(Cluster*);
  Cluster* currentCluster() const;
  const LayoutBlock* deepestBlockContainingAllText(Cluster*);
  const LayoutBlock* deepestBlockContainingAllText(const LayoutBlock*) const;
  // Returns the first text leaf that is in the current cluster. We attempt to
  // not include text from descendant clusters but because descendant clusters
  // may not exist, this is only an approximation.  The TraversalDirection
  // controls whether we return the first or the last text leaf.
  const LayoutObject* findTextLeaf(const LayoutObject*,
                                   size_t&,
                                   TextLeafSearch) const;
  BlockFlags classifyBlock(const LayoutObject*,
                           BlockFlags mask = UINT_MAX) const;
#ifdef AUTOSIZING_DOM_DEBUG_INFO
  void writeClusterDebugInfo(Cluster*);
#endif
  // Must be called at the start of layout.
  void checkSuperclusterConsistency();
  // Mark the nearest non-inheritance supercluser
  void markSuperclusterForConsistencyCheck(LayoutObject*);

  Member<const Document> m_document;
  const LayoutBlock* m_firstBlockToBeginLayout;
#if DCHECK_IS_ON()
  // Used to ensure we don't compute properties of a block before beginLayout()
  // is called on it.
  ConstBlockSet m_blocksThatHaveBegunLayout;
#endif

  // Clusters are created and destroyed during layout
  ClusterStack m_clusterStack;
  FingerprintMapper m_fingerprintMapper;
  Vector<RefPtr<ComputedStyle>> m_stylesRetainedDuringLayout;
  // FIXME: All frames should share the same m_pageInfo instance.
  PageInfo m_pageInfo;
  bool m_updatePageInfoDeferred;
};

}  // namespace blink

#endif  // TextAutosizer_h
