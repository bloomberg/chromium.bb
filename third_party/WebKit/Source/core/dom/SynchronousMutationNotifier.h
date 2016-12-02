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
class SynchronousMutationObserver;
class Text;

class CORE_EXPORT SynchronousMutationNotifier
    : public LifecycleNotifier<Document, SynchronousMutationObserver> {
 public:
  // TODO(yosin): We will have |notifyXXX()| functions defined in
  // |SynchronousMutationObserver|.
  void notifyChangeChildren(const ContainerNode&);
  void notifyMergeTextNodes(Text&, unsigned);
  void notifySplitTextNode(const Text&);
  void notifyUpdateCharacterData(CharacterData*,
                                 unsigned offset,
                                 unsigned oldLength,
                                 unsigned newLength);
  void notifyNodeChildrenWillBeRemoved(ContainerNode&);
  void notifyNodeWillBeRemoved(Node&);

 protected:
  SynchronousMutationNotifier();

 private:
  DISALLOW_COPY_AND_ASSIGN(SynchronousMutationNotifier);
};

}  // namespace dom

#endif  // SynchronousMutationNotifier_h
