// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/synchronous_mutation_notifier.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/synchronous_mutation_observer.h"

namespace blink {

SynchronousMutationNotifier::SynchronousMutationNotifier() = default;

void SynchronousMutationNotifier::NotifyChangeChildren(
    const ContainerNode& container) {
  for (LifecycleObserverBase* observer_base : observers_) {
    SynchronousMutationObserver* observer =
        static_cast<SynchronousMutationObserver*>(observer_base);
    observer->DidChangeChildren(container);
  }
}

void SynchronousMutationNotifier::NotifyMergeTextNodes(
    const Text& node,
    const NodeWithIndex& node_to_be_removed_with_index,
    unsigned old_length) {
  for (LifecycleObserverBase* observer_base : observers_) {
    SynchronousMutationObserver* observer =
        static_cast<SynchronousMutationObserver*>(observer_base);
    observer->DidMergeTextNodes(node, node_to_be_removed_with_index,
                                old_length);
  }
}

void SynchronousMutationNotifier::NotifyMoveTreeToNewDocument(
    const Node& root) {
  for (LifecycleObserverBase* observer_base : observers_) {
    SynchronousMutationObserver* observer =
        static_cast<SynchronousMutationObserver*>(observer_base);
    observer->DidMoveTreeToNewDocument(root);
  }
}

void SynchronousMutationNotifier::NotifySplitTextNode(const Text& node) {
  for (LifecycleObserverBase* observer_base : observers_) {
    SynchronousMutationObserver* observer =
        static_cast<SynchronousMutationObserver*>(observer_base);
    observer->DidSplitTextNode(node);
  }
}

void SynchronousMutationNotifier::NotifyUpdateCharacterData(
    CharacterData* character_data,
    unsigned offset,
    unsigned old_length,
    unsigned new_length) {
  for (LifecycleObserverBase* observer_base : observers_) {
    SynchronousMutationObserver* observer =
        static_cast<SynchronousMutationObserver*>(observer_base);
    observer->DidUpdateCharacterData(character_data, offset, old_length,
                                     new_length);
  }
}

void SynchronousMutationNotifier::NotifyNodeChildrenWillBeRemoved(
    ContainerNode& container) {
  for (LifecycleObserverBase* observer_base : observers_) {
    SynchronousMutationObserver* observer =
        static_cast<SynchronousMutationObserver*>(observer_base);
    observer->NodeChildrenWillBeRemoved(container);
  }
}

void SynchronousMutationNotifier::NotifyNodeWillBeRemoved(Node& node) {
  for (LifecycleObserverBase* observer_base : observers_) {
    SynchronousMutationObserver* observer =
        static_cast<SynchronousMutationObserver*>(observer_base);
    observer->NodeWillBeRemoved(node);
  }
}

}  // namespace blink
