// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/SynchronousMutationObserver.h"

#include "core/dom/Document.h"
#include "core/dom/SynchronousMutationNotifier.h"

namespace blink {

SynchronousMutationObserver::SynchronousMutationObserver()
    : LifecycleObserver(nullptr) {}

void SynchronousMutationObserver::DidChangeChildren(const ContainerNode&) {}
void SynchronousMutationObserver::DidMergeTextNodes(const Text&,
                                                    const NodeWithIndex&,
                                                    unsigned) {}
void SynchronousMutationObserver::DidMoveTreeToNewDocument(const Node&) {}
void SynchronousMutationObserver::DidSplitTextNode(const Text&) {}
void SynchronousMutationObserver::DidUpdateCharacterData(CharacterData*,
                                                         unsigned,
                                                         unsigned,
                                                         unsigned) {}
void SynchronousMutationObserver::NodeChildrenWillBeRemoved(ContainerNode&) {}
void SynchronousMutationObserver::NodeWillBeRemoved(Node&) {}

}  // namespace blink
