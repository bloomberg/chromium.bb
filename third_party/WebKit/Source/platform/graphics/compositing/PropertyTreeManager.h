// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PropertyTreeManager_h
#define PropertyTreeManager_h

#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/Noncopyable.h"
#include "wtf/Vector.h"

namespace cc {
class ClipTree;
class EffectTree;
class Layer;
class PropertyTrees;
class ScrollTree;
class TransformTree;
}

namespace blink {

class ClipPaintPropertyNode;
class EffectPaintPropertyNode;
class ScrollPaintPropertyNode;
class TransformPaintPropertyNode;

// Mutates a cc property tree to reflect Blink paint property tree
// state. Intended for use by PaintArtifactCompositor.
class PropertyTreeManager {
  WTF_MAKE_NONCOPYABLE(PropertyTreeManager);

 public:
  PropertyTreeManager(cc::PropertyTrees&,
                      cc::Layer* rootLayer,
                      int sequenceNumber);

  void setupRootTransformNode();
  void setupRootClipNode();
  void setupRootEffectNode();
  void setupRootScrollNode();

  // A brief discourse on cc property tree nodes, identifiers, and current and
  // future design evolution envisioned:
  //
  // cc property trees identify nodes by their |id|, which implementation-wise
  // is actually its index in the property tree's vector of its node type. More
  // recent cc code now refers to these as 'node indices', or 'property tree
  // indices'. |parent_id| is the same sort of 'node index' of that node's
  // parent.
  //
  // Note there are two other primary types of 'ids' referenced in cc property
  // tree related logic: (1) ElementId, also known Blink-side as
  // CompositorElementId, used by the animation system to allow tying an element
  // to its respective layer, and (2) layer ids. There are other ancillary ids
  // not relevant to any of the above, such as
  // cc::TransformNode::sorting_context_id
  // (a.k.a. blink::TransformPaintPropertyNode::renderingContextId()).
  //
  // There is a vision to move toward a world where cc property nodes have no
  // association with layers and instead have a |stable_id|. The id could come
  // from an ElementId in turn derived from the layout object responsible for
  // creating the property node.
  //
  // We would also like to explore moving to use a single shared property tree
  // representation across both cc and Blink. See
  // platform/graphics/paint/README.md for more.
  //
  // With the above as background, we can now state more clearly a description
  // of the below set of compositor node methods: they take Blink paint property
  // tree nodes as input, create a corresponding compositor property tree node
  // if none yet exists, and return the compositor node's 'node id', a.k.a.,
  // 'node index'.

  int ensureCompositorTransformNode(const TransformPaintPropertyNode*);
  int ensureCompositorClipNode(const ClipPaintPropertyNode*);
  // Update the layer->scroll and scroll->layer mapping. The latter is temporary
  // until |owning_layer_id| is removed from the scroll node.
  void updateLayerScrollMapping(cc::Layer*, const TransformPaintPropertyNode*);

  int switchToEffectNode(const EffectPaintPropertyNode& nextEffect);
  int getCurrentCompositorEffectNodeIndex() const {
    return m_effectStack.back().id;
  }

 private:
  void buildEffectNodesRecursively(const EffectPaintPropertyNode* nextEffect);

  cc::TransformTree& transformTree();
  cc::ClipTree& clipTree();
  cc::EffectTree& effectTree();
  cc::ScrollTree& scrollTree();

  int ensureCompositorScrollNode(const ScrollPaintPropertyNode*);

  const EffectPaintPropertyNode* currentEffectNode() const;

  // Scroll translation has special treatment in the transform and scroll trees.
  void updateScrollAndScrollTranslationNodes(const TransformPaintPropertyNode*);

  // Property trees which should be updated by the manager.
  cc::PropertyTrees& m_propertyTrees;

  // Layer to which transform "owner" layers should be added. These will not
  // have any actual children, but at present must exist in the tree.
  cc::Layer* m_rootLayer;

  // Maps from Blink-side property tree nodes to cc property node indices.
  HashMap<const TransformPaintPropertyNode*, int> m_transformNodeMap;
  HashMap<const ClipPaintPropertyNode*, int> m_clipNodeMap;
  HashMap<const ScrollPaintPropertyNode*, int> m_scrollNodeMap;

  struct BlinkEffectAndCcIdPair {
    const EffectPaintPropertyNode* effect;
    // The cc property tree effect node id, or 'node index', for the cc effect
    // node corresponding to the above Blink effect paint property node.
    int id;
  };
  Vector<BlinkEffectAndCcIdPair> m_effectStack;
  int m_sequenceNumber;

#if DCHECK_IS_ON()
  HashSet<const EffectPaintPropertyNode*> m_effectNodesConverted;
#endif
};

}  // namespace blink

#endif  // PropertyTreeManager_h
