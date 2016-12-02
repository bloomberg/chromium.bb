// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/SynchronousMutationNotifier.h"

#include "core/dom/Document.h"
#include "core/dom/SynchronousMutationObserver.h"

namespace blink {

SynchronousMutationNotifier::SynchronousMutationNotifier() = default;

void SynchronousMutationNotifier::notifyChangeChildren(
    const ContainerNode& container) {
  for (SynchronousMutationObserver* observer : m_observers)
    observer->didChangeChildren(container);
}

void SynchronousMutationNotifier::notifyMergeTextNodes(Text& node,
                                                       unsigned offset) {
  for (SynchronousMutationObserver* observer : m_observers)
    observer->didMergeTextNodes(node, offset);
}

void SynchronousMutationNotifier::notifySplitTextNode(const Text& node) {
  for (SynchronousMutationObserver* observer : m_observers)
    observer->didSplitTextNode(node);
}

void SynchronousMutationNotifier::notifyUpdateCharacterData(
    CharacterData* characterData,
    unsigned offset,
    unsigned oldLength,
    unsigned newLength) {
  for (SynchronousMutationObserver* observer : m_observers) {
    observer->didUpdateCharacterData(characterData, offset, oldLength,
                                     newLength);
  }
}

void SynchronousMutationNotifier::notifyNodeChildrenWillBeRemoved(
    ContainerNode& container) {
  for (SynchronousMutationObserver* observer : m_observers)
    observer->nodeChildrenWillBeRemoved(container);
}

void SynchronousMutationNotifier::notifyNodeWillBeRemoved(Node& node) {
  for (SynchronousMutationObserver* observer : m_observers)
    observer->nodeWillBeRemoved(node);
}

}  // namespace blink
