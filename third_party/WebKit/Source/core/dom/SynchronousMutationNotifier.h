// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SynchronousMutationNotifier_h
#define SynchronousMutationNotifier_h

#include "base/macros.h"
#include "core/CoreExport.h"
#include "platform/LifecycleNotifier.h"

namespace blink {

class CharacterData;
class ContainerNode;
class Document;
class Node;
class NodeWithIndex;
class SynchronousMutationObserver;
class Text;

class CORE_EXPORT SynchronousMutationNotifier
    : public LifecycleNotifier<Document, SynchronousMutationObserver> {
 public:
  // TODO(yosin): We will have |notifyXXX()| functions defined in
  // |SynchronousMutationObserver|.
  void NotifyChangeChildren(const ContainerNode&);
  void NotifyMergeTextNodes(const Text& merged_node,
                            const NodeWithIndex& node_to_be_removed_with_index,
                            unsigned old_length);
  void NotifyMoveTreeToNewDocument(const Node&);
  void NotifySplitTextNode(const Text&);
  void NotifyUpdateCharacterData(CharacterData*,
                                 unsigned offset,
                                 unsigned old_length,
                                 unsigned new_length);
  void NotifyNodeChildrenWillBeRemoved(ContainerNode&);
  void NotifyNodeWillBeRemoved(Node&);

 protected:
  SynchronousMutationNotifier();

 private:
  DISALLOW_COPY_AND_ASSIGN(SynchronousMutationNotifier);
};

}  // namespace dom

#endif  // SynchronousMutationNotifier_h
