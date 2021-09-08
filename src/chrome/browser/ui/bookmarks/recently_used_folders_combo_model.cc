// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/recently_used_folders_combo_model.h"

#include <stddef.h>

#include "base/metrics/user_metrics.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model_observer.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace {

// Max number of most recently used folders.
const size_t kMaxMRUFolders = 5;

}  // namespace

struct RecentlyUsedFoldersComboModel::Item {
  enum Type {
    TYPE_NODE,
    TYPE_SEPARATOR,
    TYPE_CHOOSE_ANOTHER_FOLDER
  };

  Item(const BookmarkNode* node, Type type);
  ~Item();

  bool operator==(const Item& item) const;

  const BookmarkNode* node;
  Type type;
};

RecentlyUsedFoldersComboModel::Item::Item(const BookmarkNode* node,
                                          Type type)
    : node(node),
      type(type) {
}

RecentlyUsedFoldersComboModel::Item::~Item() = default;

bool RecentlyUsedFoldersComboModel::Item::operator==(const Item& item) const {
  return item.node == node && item.type == type;
}

RecentlyUsedFoldersComboModel::RecentlyUsedFoldersComboModel(
    BookmarkModel* model,
    const BookmarkNode* node)
    : bookmark_model_(model), parent_node_(node->parent()) {
  bookmark_model_->AddObserver(this);
  // Use + 2 to account for bookmark bar and other node.
  std::vector<const BookmarkNode*> nodes =
      bookmarks::GetMostRecentlyModifiedUserFolders(model, kMaxMRUFolders + 2);

  for (size_t i = 0; i < nodes.size(); ++i)
    items_.push_back(Item(nodes[i], Item::TYPE_NODE));

  // We special case the placement of these, so remove them from the list, then
  // fix up the order.
  RemoveNode(model->bookmark_bar_node());
  RemoveNode(model->mobile_node());
  RemoveNode(model->other_node());
  RemoveNode(node->parent());

  // Make the parent the first item, unless it's a permanent node, which is
  // added below.
  if (!model->is_permanent_node(node->parent()))
    items_.insert(items_.begin(), Item(node->parent(), Item::TYPE_NODE));

  // Make sure we only have kMaxMRUFolders in the first chunk.
  if (items_.size() > kMaxMRUFolders)
    items_.erase(items_.begin() + kMaxMRUFolders, items_.end());

  // And put the bookmark bar and other nodes at the end of the list.
  items_.push_back(Item(model->bookmark_bar_node(), Item::TYPE_NODE));
  items_.push_back(Item(model->other_node(), Item::TYPE_NODE));
  if (model->mobile_node()->IsVisible())
    items_.push_back(Item(model->mobile_node(), Item::TYPE_NODE));
  items_.push_back(Item(NULL, Item::TYPE_SEPARATOR));
  items_.push_back(Item(NULL, Item::TYPE_CHOOSE_ANOTHER_FOLDER));
}

RecentlyUsedFoldersComboModel::~RecentlyUsedFoldersComboModel() {
  bookmark_model_->RemoveObserver(this);
}

int RecentlyUsedFoldersComboModel::GetItemCount() const {
  return static_cast<int>(items_.size());
}

std::u16string RecentlyUsedFoldersComboModel::GetItemAt(int index) const {
  switch (items_[index].type) {
    case Item::TYPE_NODE:
      return items_[index].node->GetTitle();
    case Item::TYPE_SEPARATOR:
      // This function should not be called for separators.
      NOTREACHED();
      return std::u16string();
    case Item::TYPE_CHOOSE_ANOTHER_FOLDER:
      return l10n_util::GetStringUTF16(
          IDS_BOOKMARK_BUBBLE_CHOOSER_ANOTHER_FOLDER);
  }
  NOTREACHED();
  return std::u16string();
}

bool RecentlyUsedFoldersComboModel::IsItemSeparatorAt(int index) const {
  return items_[index].type == Item::TYPE_SEPARATOR;
}

int RecentlyUsedFoldersComboModel::GetDefaultIndex() const {
  // TODO(pbos): Ideally we shouldn't have to handle `parent_node_` removal
  // here, the dialog should instead close immediately (and destroy `this`).
  // If that can be resolved, this should DCHECK that it != items_.end() and
  // a DCHECK should be added in the BookmarkModel observer methods to ensure
  // that we don't remove `parent_node_`.
  // TODO(pbos): Look at returning -1 here if there's no default index. Right
  // now a lot of code in Combobox assumes an index within `items_` bounds.
  auto it = std::find(items_.begin(), items_.end(),
                      Item(parent_node_, Item::TYPE_NODE));
  return it == items_.end() ? 0 : static_cast<int>(it - items_.begin());
}

void RecentlyUsedFoldersComboModel::AddObserver(
    ui::ComboboxModelObserver* observer) {
  observers_.AddObserver(observer);
}

void RecentlyUsedFoldersComboModel::RemoveObserver(
    ui::ComboboxModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void RecentlyUsedFoldersComboModel::BookmarkModelLoaded(BookmarkModel* model,
                                                        bool ids_reassigned) {}

void RecentlyUsedFoldersComboModel::BookmarkModelBeingDeleted(
    BookmarkModel* model) {
}

void RecentlyUsedFoldersComboModel::BookmarkNodeMoved(
    BookmarkModel* model,
    const BookmarkNode* old_parent,
    size_t old_index,
    const BookmarkNode* new_parent,
    size_t new_index) {}

void RecentlyUsedFoldersComboModel::BookmarkNodeAdded(
    BookmarkModel* model,
    const BookmarkNode* parent,
    size_t index) {}

void RecentlyUsedFoldersComboModel::OnWillRemoveBookmarks(
    BookmarkModel* model,
    const BookmarkNode* parent,
    size_t old_index,
    const BookmarkNode* node) {
  // Changing is rare enough that we don't attempt to readjust the contents.
  // Update |items_| so we aren't left pointing to a deleted node.
  bool changed = false;
  for (auto i = items_.begin(); i != items_.end();) {
    if (i->type == Item::TYPE_NODE && i->node->HasAncestor(node)) {
      i = items_.erase(i);
      changed = true;
    } else {
      ++i;
    }
  }
  if (changed) {
    for (ui::ComboboxModelObserver& observer : observers_)
      observer.OnComboboxModelChanged(this);
  }
}

void RecentlyUsedFoldersComboModel::BookmarkNodeRemoved(
    BookmarkModel* model,
    const BookmarkNode* parent,
    size_t old_index,
    const BookmarkNode* node,
    const std::set<GURL>& removed_urls) {}

void RecentlyUsedFoldersComboModel::BookmarkNodeChanged(
    BookmarkModel* model,
    const BookmarkNode* node) {
}

void RecentlyUsedFoldersComboModel::BookmarkNodeFaviconChanged(
    BookmarkModel* model,
    const BookmarkNode* node) {
}

void RecentlyUsedFoldersComboModel::BookmarkNodeChildrenReordered(
      BookmarkModel* model,
      const BookmarkNode* node) {
}

void RecentlyUsedFoldersComboModel::BookmarkAllUserNodesRemoved(
    BookmarkModel* model,
    const std::set<GURL>& removed_urls) {
  // Changing is rare enough that we don't attempt to readjust the contents.
  // Update |items_| so we aren't left pointing to a deleted node.
  bool changed = false;
  for (auto i = items_.begin(); i != items_.end();) {
    if (i->type == Item::TYPE_NODE &&
        !bookmark_model_->is_permanent_node(i->node)) {
      i = items_.erase(i);
      changed = true;
    } else {
      ++i;
    }
  }
  if (changed) {
    for (ui::ComboboxModelObserver& observer : observers_)
      observer.OnComboboxModelChanged(this);
  }
}

void RecentlyUsedFoldersComboModel::MaybeChangeParent(
    const BookmarkNode* node,
    int selected_index) {
  DCHECK_LT(selected_index, static_cast<int>(items_.size()));
  if (items_[selected_index].type != Item::TYPE_NODE)
    return;

  const BookmarkNode* new_parent = GetNodeAt(selected_index);
  if (new_parent != node->parent()) {
    base::RecordAction(base::UserMetricsAction("BookmarkBubble_ChangeParent"));
    bookmark_model_->Move(node, new_parent, new_parent->children().size());
  }
}

const BookmarkNode* RecentlyUsedFoldersComboModel::GetNodeAt(int index) {
  if (index < 0 || index >= static_cast<int>(items_.size()))
    return nullptr;
  return items_[index].node;
}

void RecentlyUsedFoldersComboModel::RemoveNode(const BookmarkNode* node) {
  auto it =
      std::find(items_.begin(), items_.end(), Item(node, Item::TYPE_NODE));
  if (it != items_.end())
    items_.erase(it);
}
