// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/item_tree.h"

#include <algorithm>

#include "base/stl_util.h"
#include "tools/gn/err.h"
#include "tools/gn/item.h"
#include "tools/gn/item_node.h"

namespace {

// Recursively looks in the tree for a given node, returning true if it
// was found in the dependecy graph. This is used to see if a given node
// participates in a cycle.
//
// Note that "look_for" and "search_in" will be the same node when starting the
// search, so we don't want to return true in that case.
//
// If a cycle is found, the return value will be true and the cycle vector will
// be filled with the path (in reverse order).
bool RecursiveFindCycle(const ItemNode* look_for,
                        const ItemNode* search_in,
                        std::vector<const ItemNode*>* cycle) {
  const ItemNode::ItemNodeSet& unresolved =
      search_in->unresolved_dependencies();
  for (ItemNode::ItemNodeSet::const_iterator i = unresolved.begin();
       i != unresolved.end(); ++i) {
    if (*i == look_for) {
      cycle->push_back(*i);
      return true;
    }

    if (RecursiveFindCycle(look_for, *i, cycle)) {
      // Found a cycle inside this one, record our path and return.
      cycle->push_back(*i);
      return true;
    }
  }
  return false;
}

}  // namespace

ItemTree::ItemTree() {
}

ItemTree::~ItemTree() {
  STLDeleteContainerPairSecondPointers(items_.begin(), items_.end());
}

ItemNode* ItemTree::GetExistingNodeLocked(const Label& label) {
  lock_.AssertAcquired();
  StringToNodeHash::iterator found = items_.find(label);
  if (found == items_.end())
    return NULL;
  return found->second;
}

void ItemTree::AddNodeLocked(ItemNode* node) {
  lock_.AssertAcquired();
  DCHECK(items_.find(node->item()->label()) == items_.end());
  items_[node->item()->label()] = node;
}

Err ItemTree::MarkItemGeneratedLocked(const Label& label) {
  lock_.AssertAcquired();
  DCHECK(items_.find(label) != items_.end());

  ItemNode* node = items_[label];

  if (!node->unresolved_dependencies().empty()) {
    // Still some pending dependencies, wait for those to be resolved.
    node->SetGenerated();
    return Err();
  }
  return MarkItemResolvedLocked(node);
}

void ItemTree::GetAllItemsLocked(std::vector<const Item*>* dest) const {
  lock_.AssertAcquired();
  dest->reserve(items_.size());
  for (StringToNodeHash::const_iterator i = items_.begin();
       i != items_.end(); ++i) {
    dest->push_back(i->second->item());
  }
}

Err ItemTree::CheckForBadItems() const {
  base::AutoLock lock(lock_);

  // Look for errors where we find a GENERATED node that refers to a REFERENCED
  // one. There may be other nodes depending on the GENERATED one, but listing
  // all of those isn't helpful, we want to find the broken link.
  //
  // This finds normal "missing dependency" errors but does not find circular
  // dependencies because in this case all items in the cycle will be GENERATED
  // but none will be resolved. If this happens, we'll check explicitly for
  // that below.
  std::vector<const ItemNode*> bad_nodes;
  std::string depstring;
  for (StringToNodeHash::const_iterator i = items_.begin();
       i != items_.end(); ++i) {
    const ItemNode* src = i->second;

    if (src->state() == ItemNode::GENERATED) {
      bad_nodes.push_back(src);

      // Check dependencies.
      for (ItemNode::ItemNodeSet::const_iterator dest =
               src->unresolved_dependencies().begin();
          dest != src->unresolved_dependencies().end();
          ++dest) {
        if ((*dest)->state() == ItemNode::REFERENCED) {
          depstring += "\"" + src->item()->label().GetUserVisibleName(false) +
              "\" needs " + (*dest)->item()->GetItemTypeName() +
              " \"" + (*dest)->item()->label().GetUserVisibleName(false) +
              "\"\n";
        }
      }
    }
  }

  if (!bad_nodes.empty() && depstring.empty()) {
    // Our logic above found a bad node but didn't identify the problem. This
    // normally means a circular dependency.
    depstring = CheckForCircularDependenciesLocked(bad_nodes);
    if (depstring.empty()) {
      // Something's very wrong, just dump out the bad nodes.
      depstring = "I have no idea what went wrong, but these are unresolved, "
          "possible due to an\ninternal error:";
      for (size_t i = 0; i < bad_nodes.size(); i++) {
        depstring += "\n\"" +
            bad_nodes[i]->item()->label().GetUserVisibleName(false) + "\"";
      }
    }
  }

  if (depstring.empty())
    return Err();
  return Err(Location(), "Unresolved dependencies.", depstring);
}

Err ItemTree::MarkItemResolvedLocked(ItemNode* node) {
  node->SetResolved();
  node->item()->OnResolved();

  // Now update our waiters, pushing the "resolved" bit.
  ItemNode::ItemNodeSet waiting;
  node->SwapOutWaitingDependencySet(&waiting);
  for (ItemNode::ItemNodeSet::iterator i = waiting.begin();
       i != waiting.end(); ++i) {
    ItemNode* waiter = *i;

    // Our node should be unresolved in the waiter.
    DCHECK(waiter->unresolved_dependencies().find(node) !=
           waiter->unresolved_dependencies().end());
    waiter->MarkDirectDependencyResolved(node);

    // Recursively mark nodes as resolved.
    if (waiter->state() == ItemNode::GENERATED &&
        waiter->unresolved_dependencies().empty()) {
      Err err = MarkItemResolvedLocked(waiter);
      if (err.has_error())
        return err;
    }
  }

  return Err();
}

std::string ItemTree::CheckForCircularDependenciesLocked(
    const std::vector<const ItemNode*>& bad_nodes) const {
  std::vector<const ItemNode*> cycle;
  if (!RecursiveFindCycle(bad_nodes[0], bad_nodes[0], &cycle))
    return std::string();  // Didn't find a cycle, something else is wrong.

  cycle.push_back(bad_nodes[0]);
  std::string ret = "There is a dependency cycle:";

  // Walk backwards since the dependency arrows point in the reverse direction.
  for (int i = static_cast<int>(cycle.size()) - 1; i >= 0; i--) {
    ret += "\n  \"" + cycle[i]->item()->label().GetUserVisibleName(false) +
        "\"";
    if (i != 0)
      ret += " ->";
  }

  return ret;
}
