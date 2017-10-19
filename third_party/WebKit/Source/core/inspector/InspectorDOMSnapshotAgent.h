// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorDOMSnapshotAgent_h
#define InspectorDOMSnapshotAgent_h

#include "core/CSSPropertyNames.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/protocol/DOMSnapshot.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/Vector.h"

namespace blink {

class Element;
class InspectedFrames;
class Node;

class CORE_EXPORT InspectorDOMSnapshotAgent final
    : public InspectorBaseAgent<protocol::DOMSnapshot::Metainfo> {
  WTF_MAKE_NONCOPYABLE(InspectorDOMSnapshotAgent);

 public:
  static InspectorDOMSnapshotAgent* Create(InspectedFrames* inspected_frames) {
    return new InspectorDOMSnapshotAgent(inspected_frames);
  }

  ~InspectorDOMSnapshotAgent() override;
  virtual void Trace(blink::Visitor*);

  protocol::Response getSnapshot(
      std::unique_ptr<protocol::Array<String>> style_whitelist,
      std::unique_ptr<protocol::Array<protocol::DOMSnapshot::DOMNode>>*
          dom_nodes,
      std::unique_ptr<protocol::Array<protocol::DOMSnapshot::LayoutTreeNode>>*
          layout_tree_nodes,
      std::unique_ptr<protocol::Array<protocol::DOMSnapshot::ComputedStyle>>*
          computed_styles) override;

 private:
  explicit InspectorDOMSnapshotAgent(InspectedFrames*);

  // Adds a DOMNode for the given Node to |dom_nodes_| and returns its index.
  int VisitNode(Node*);
  std::unique_ptr<protocol::Array<int>> VisitContainerChildren(Node* container);
  std::unique_ptr<protocol::Array<int>> VisitPseudoElements(Element* parent);
  std::unique_ptr<protocol::Array<protocol::DOMSnapshot::NameValue>>
  BuildArrayForElementAttributes(Element*);

  // Adds a LayoutTreeNode for the LayoutObject of the given Node to
  // |layout_tree_nodes_| and returns its index. Returns -1 if the Node has no
  // associated LayoutObject.
  int VisitLayoutTreeNode(Node*, int node_index);

  // Returns the index of the ComputedStyle in |computed_styles_| for the given
  // Node. Adds a new ComputedStyle if necessary, but ensures no duplicates are
  // added to |computed_styles_|. Returns -1 if the node has no values for
  // styles in |style_whitelist_|.
  int GetStyleIndexForNode(Node*);

  struct VectorStringHashTraits;
  using ComputedStylesMap = WTF::HashMap<Vector<String>,
                                         int,
                                         VectorStringHashTraits,
                                         VectorStringHashTraits>;
  using CSSPropertyWhitelist = Vector<std::pair<String, CSSPropertyID>>;

  // State of current snapshot.
  std::unique_ptr<protocol::Array<protocol::DOMSnapshot::DOMNode>> dom_nodes_;
  std::unique_ptr<protocol::Array<protocol::DOMSnapshot::LayoutTreeNode>>
      layout_tree_nodes_;
  std::unique_ptr<protocol::Array<protocol::DOMSnapshot::ComputedStyle>>
      computed_styles_;
  // Maps a style string vector to an index in |computed_styles_|. Used to avoid
  // duplicate entries in |computed_styles_|.
  std::unique_ptr<ComputedStylesMap> computed_styles_map_;
  std::unique_ptr<Vector<std::pair<String, CSSPropertyID>>>
      css_property_whitelist_;

  Member<InspectedFrames> inspected_frames_;
};

}  // namespace blink

#endif  // !defined(InspectorDOMSnapshotAgent_h)
