// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/find_cc_layer.h"

#include "cc/layers/layer.h"
#include "cc/layers/scrollbar_layer_base.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/scroll_node.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace blink {

Vector<const cc::Layer*> CcLayersByName(const cc::Layer* root,
                                        const String& name_regex) {
  Vector<const cc::Layer*> layers;
  ::testing::Matcher<std::string> matcher(
      ::testing::ContainsRegex(name_regex.Utf8()));
  for (auto& layer : root->children()) {
    if (matcher.Matches(layer->DebugName()))
      layers.push_back(layer.get());
  }
  return layers;
}

Vector<const cc::Layer*> CcLayersByDOMElementId(const cc::Layer* root,
                                                const String& dom_id) {
  return CcLayersByName(root, String("id='") + dom_id + "'");
}

const cc::Layer* CcLayerByCcElementId(const cc::Layer* root,
                                      const CompositorElementId& element_id) {
  return root->layer_tree_host()->LayerByElementId(element_id);
}

const cc::Layer* ScrollingContentsCcLayerByScrollElementId(
    const cc::Layer* root,
    const CompositorElementId& scroll_element_id) {
  const auto& scroll_tree =
      root->layer_tree_host()->property_trees()->scroll_tree;
  for (auto& layer : root->children()) {
    const auto* scroll_node = scroll_tree.Node(layer->scroll_tree_index());
    if (scroll_node && scroll_node->element_id == scroll_element_id &&
        scroll_node->transform_id == layer->transform_tree_index())
      return layer.get();
  }
  return nullptr;
}

const cc::ScrollbarLayerBase* ScrollbarLayerForScrollNode(
    const cc::Layer* root,
    const cc::ScrollNode* scroll_node,
    cc::ScrollbarOrientation orientation) {
  if (!scroll_node)
    return nullptr;
  for (auto& layer : root->children()) {
    if (!layer->is_scrollbar())
      continue;
    const auto* scrollbar_layer =
        static_cast<const cc::ScrollbarLayerBase*>(layer.get());
    if (scrollbar_layer->scroll_element_id() == scroll_node->element_id &&
        scrollbar_layer->orientation() == orientation)
      return scrollbar_layer;
  }
  return nullptr;
}

}  // namespace blink
