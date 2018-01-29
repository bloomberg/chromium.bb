// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ChildFrameDisconnector.h"

#include "core/dom/ElementShadow.h"
#include "core/dom/ShadowRoot.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "platform/wtf/Assertions.h"

namespace blink {

#if DCHECK_IS_ON()
static unsigned CheckConnectedSubframeCountIsConsistent(Node&);
#endif

void ChildFrameDisconnector::Disconnect(DisconnectPolicy policy) {
#if DCHECK_IS_ON()
  CheckConnectedSubframeCountIsConsistent(Root());
#endif

  if (!Root().ConnectedSubframeCount())
    return;

  if (policy == kRootAndDescendants) {
    CollectFrameOwners(Root());
  } else {
    for (Node* child = Root().firstChild(); child; child = child->nextSibling())
      CollectFrameOwners(*child);
  }

  DisconnectCollectedFrameOwners();
}

void ChildFrameDisconnector::CollectFrameOwners(Node& root) {
  if (!root.ConnectedSubframeCount())
    return;

  if (root.IsHTMLElement() && root.IsFrameOwnerElement())
    frame_owners_.push_back(&ToHTMLFrameOwnerElement(root));

  for (Node* child = root.firstChild(); child; child = child->nextSibling())
    CollectFrameOwners(*child);

  ElementShadow* shadow =
      root.IsElementNode() ? ToElement(root).Shadow() : nullptr;
  if (shadow)
    CollectFrameOwners(*shadow);
}

void ChildFrameDisconnector::DisconnectCollectedFrameOwners() {
  // Must disable frame loading in the subtree so an unload handler cannot
  // insert more frames and create loaded frames in detached subtrees.
  SubframeLoadingDisabler disabler(Root());

  for (unsigned i = 0; i < frame_owners_.size(); ++i) {
    HTMLFrameOwnerElement* owner = frame_owners_[i].Get();
    // Don't need to traverse up the tree for the first owner since no
    // script could have moved it.
    if (!i || Root().IsShadowIncludingInclusiveAncestorOf(owner))
      owner->DisconnectContentFrame();
  }
}

void ChildFrameDisconnector::CollectFrameOwners(ElementShadow& shadow) {
  for (ShadowRoot* root = &shadow.YoungestShadowRoot(); root;
       root = root->OlderShadowRoot())
    CollectFrameOwners(*root);
}

#if DCHECK_IS_ON()
static unsigned CheckConnectedSubframeCountIsConsistent(Node& node) {
  unsigned count = 0;

  if (node.IsElementNode()) {
    if (node.IsFrameOwnerElement() &&
        ToHTMLFrameOwnerElement(node).ContentFrame())
      count++;

    if (ElementShadow* shadow = ToElement(node).Shadow()) {
      for (ShadowRoot* root = &shadow->YoungestShadowRoot(); root;
           root = root->OlderShadowRoot())
        count += CheckConnectedSubframeCountIsConsistent(*root);
    }
  }

  for (Node* child = node.firstChild(); child; child = child->nextSibling())
    count += CheckConnectedSubframeCountIsConsistent(*child);

  // If we undercount there's possibly a security bug since we'd leave frames
  // in subtrees outside the document.
  DCHECK_GE(node.ConnectedSubframeCount(), count);

  // If we overcount it's safe, but not optimal because it means we'll traverse
  // through the document in ChildFrameDisconnector looking for frames that have
  // already been disconnected.
  DCHECK_EQ(node.ConnectedSubframeCount(), count);

  return count;
}
#endif

}  // namespace blink
