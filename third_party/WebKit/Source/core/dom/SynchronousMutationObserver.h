// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SynchronousMutationObserver_h
#define SynchronousMutationObserver_h

#include "base/macros.h"
#include "core/CoreExport.h"
#include "platform/LifecycleObserver.h"

namespace blink {

class CharacterData;
class ContainerNode;
class Document;
class NodeWithIndex;
class Text;

// This class is a base class for classes which observe DOM tree mutation
// synchronously. If you want to observe DOM tree mutation asynchronously see
// MutationObserver Web API.
//
// TODO(yosin): Following classes should be derived from this class to
// simplify Document class.
//  - DragCaret
//  - DocumentMarkerController
//  - EventHandler
//  - FrameCaret
//  - InputMethodController
//  - SelectionController
//  - Range set
//  - NodeIterator set
class CORE_EXPORT SynchronousMutationObserver
    : public LifecycleObserver<Document, SynchronousMutationObserver> {
 public:
  // TODO(yosin): We will have following member functions:
  //  - dataWillBeChanged(const CharacterData&);
  //  - didMoveTreeToNewDocument(const Node& root);
  //  - didInsertText(Node*, unsigned offset, unsigned length);
  //  - didRemoveText(Node*, unsigned offset, unsigned length);

  // Called after child nodes changed.
  virtual void didChangeChildren(const ContainerNode&);

  // Called after characters in |nodeToBeRemoved| is appended into |mergedNode|.
  // |oldLength| holds length of |mergedNode| before merge.
  virtual void didMergeTextNodes(const Text& mergedNode,
                                 const NodeWithIndex& nodeToBeRemovedWithIndex,
                                 unsigned oldLength);

  // Called just after node tree |root| is moved to new document.
  virtual void didMoveTreeToNewDocument(const Node& root);

  // Called when |Text| node is split, next sibling |oldNode| contains
  // characters after split point.
  virtual void didSplitTextNode(const Text& oldNode);

  // Called when |CharacterData| is updated at |offset|, |oldLength| is a
  // number of deleted character and |newLength| is a number of added
  // characters.
  virtual void didUpdateCharacterData(CharacterData*,
                                      unsigned offset,
                                      unsigned oldLength,
                                      unsigned newLength);

  // Called before removing container node.
  virtual void nodeChildrenWillBeRemoved(ContainerNode&);

  // Called before removing node.
  virtual void nodeWillBeRemoved(Node&);

  // Called when detaching document.
  virtual void contextDestroyed(Document*) {}

 protected:
  SynchronousMutationObserver();

 private:
  DISALLOW_COPY_AND_ASSIGN(SynchronousMutationObserver);
};

}  // namespace blink

#endif  // SynchronousMutationObserver_h
