// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_model.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/check_op.h"
#include "base/guid.h"
#include "base/i18n/string_compare.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "components/bookmarks/browser/bookmark_expanded_state_tracker.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/bookmarks/browser/bookmark_storage.h"
#include "components/bookmarks/browser/bookmark_undo_delegate.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/browser/model_loader.h"
#include "components/bookmarks/browser/titled_url_index.h"
#include "components/bookmarks/browser/titled_url_match.h"
#include "components/bookmarks/browser/typed_count_sorter.h"
#include "components/bookmarks/browser/url_and_title.h"
#include "components/bookmarks/browser/url_index.h"
#include "components/bookmarks/common/bookmark_constants.h"
#include "components/favicon_base/favicon_types.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/favicon_size.h"

using base::Time;

namespace bookmarks {

namespace {

// Helper to get a mutable bookmark node.
BookmarkNode* AsMutable(const BookmarkNode* node) {
  return const_cast<BookmarkNode*>(node);
}

// Comparator used when sorting permanent nodes. Nodes that are initially
// visible are sorted before nodes that are initially hidden.
class VisibilityComparator {
 public:
  explicit VisibilityComparator(BookmarkClient* client) : client_(client) {}

  // Returns true if |n1| precedes |n2|.
  bool operator()(const std::unique_ptr<BookmarkNode>& n1,
                  const std::unique_ptr<BookmarkNode>& n2) {
    DCHECK(n1->is_permanent_node());
    DCHECK(n2->is_permanent_node());
    bool n1_visible = client_->IsPermanentNodeVisibleWhenEmpty(n1->type());
    bool n2_visible = client_->IsPermanentNodeVisibleWhenEmpty(n2->type());
    return n1_visible != n2_visible && n1_visible;
  }

 private:
  BookmarkClient* client_;
};

// Comparator used when sorting bookmarks. Folders are sorted first, then
// bookmarks.
class SortComparator {
 public:
  explicit SortComparator(icu::Collator* collator) : collator_(collator) {}

  // Returns true if |n1| precedes |n2|.
  bool operator()(const std::unique_ptr<BookmarkNode>& n1,
                  const std::unique_ptr<BookmarkNode>& n2) {
    if (n1->type() == n2->type()) {
      // Types are the same, compare the names.
      if (!collator_)
        return n1->GetTitle() < n2->GetTitle();
      return base::i18n::CompareString16WithCollator(
                 *collator_, n1->GetTitle(), n2->GetTitle()) == UCOL_LESS;
    }
    // Types differ, sort such that folders come first.
    return n1->is_folder();
  }

 private:
  icu::Collator* collator_;
};

// Delegate that does nothing.
class EmptyUndoDelegate : public BookmarkUndoDelegate {
 public:
  EmptyUndoDelegate() {}
  ~EmptyUndoDelegate() override {}

 private:
  // BookmarkUndoDelegate:
  void SetUndoProvider(BookmarkUndoProvider* provider) override {}
  void OnBookmarkNodeRemoved(BookmarkModel* model,
                             const BookmarkNode* parent,
                             size_t index,
                             std::unique_ptr<BookmarkNode> node) override {}

  DISALLOW_COPY_AND_ASSIGN(EmptyUndoDelegate);
};

#if DCHECK_IS_ON()
void AddGuidsToIndexRecursive(const BookmarkNode* node,
                              std::set<std::string>* guid_index) {
  bool success = guid_index->insert(node->guid()).second;
  DCHECK(success);

  // Recurse through children.
  for (size_t i = node->children().size(); i > 0; --i)
    AddGuidsToIndexRecursive(node->children()[i - 1].get(), guid_index);
}
#endif  // DCHECK_IS_ON()

}  // namespace

// BookmarkModel --------------------------------------------------------------

BookmarkModel::BookmarkModel(std::unique_ptr<BookmarkClient> client)
    : client_(std::move(client)),
      owned_root_(std::make_unique<BookmarkNode>(/*id=*/0,
                                                 BookmarkNode::RootNodeGuid(),
                                                 GURL())),
      root_(owned_root_.get()),
      observers_(base::ObserverListPolicy::EXISTING_ONLY),
      empty_undo_delegate_(std::make_unique<EmptyUndoDelegate>()) {
  DCHECK(client_);
  client_->Init(this);
}

BookmarkModel::~BookmarkModel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (BookmarkModelObserver& observer : observers_)
    observer.BookmarkModelBeingDeleted(this);

  if (store_) {
    // The store maintains a reference back to us. We need to tell it we're gone
    // so that it doesn't try and invoke a method back on us again.
    store_->BookmarkModelDeleted();
  }
}

void BookmarkModel::Load(
    PrefService* pref_service,
    const base::FilePath& profile_path,
    const scoped_refptr<base::SequencedTaskRunner>& io_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& ui_task_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If the store is non-null, it means Load was already invoked. Load should
  // only be invoked once.
  DCHECK(!store_);

  expanded_state_tracker_ =
      std::make_unique<BookmarkExpandedStateTracker>(this, pref_service);

  store_ = std::make_unique<BookmarkStorage>(this, profile_path,
                                             io_task_runner.get());
  // Creating ModelLoader schedules the load on |io_task_runner|.
  model_loader_ = ModelLoader::Create(
      profile_path.Append(kBookmarksFileName), io_task_runner.get(),
      std::make_unique<BookmarkLoadDetails>(client_.get()),
      base::BindOnce(&BookmarkModel::DoneLoading, AsWeakPtr()));
}

scoped_refptr<ModelLoader> BookmarkModel::model_loader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return model_loader_;
}

void BookmarkModel::AddObserver(BookmarkModelObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void BookmarkModel::RemoveObserver(BookmarkModelObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void BookmarkModel::BeginExtensiveChanges() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (++extensive_changes_ == 1) {
    for (BookmarkModelObserver& observer : observers_)
      observer.ExtensiveBookmarkChangesBeginning(this);
  }
}

void BookmarkModel::EndExtensiveChanges() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  --extensive_changes_;
  DCHECK_GE(extensive_changes_, 0);
  if (extensive_changes_ == 0) {
    for (BookmarkModelObserver& observer : observers_)
      observer.ExtensiveBookmarkChangesEnded(this);
  }
}

void BookmarkModel::BeginGroupedChanges() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (BookmarkModelObserver& observer : observers_)
    observer.GroupedBookmarkChangesBeginning(this);
}

void BookmarkModel::EndGroupedChanges() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (BookmarkModelObserver& observer : observers_)
    observer.GroupedBookmarkChangesEnded(this);
}

void BookmarkModel::Remove(const BookmarkNode* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded_);
  DCHECK(node);
  DCHECK(!is_root_node(node));
  const BookmarkNode* parent = node->parent();
  DCHECK(parent);
  size_t index = size_t{parent->GetIndexOf(node)};
  DCHECK_NE(size_t{-1}, index);

  // Removing a permanent node is problematic and can cause crashes elsewhere
  // that are difficult to trace back.
  CHECK(!is_permanent_node(node)) << "for type " << node->type();

  for (BookmarkModelObserver& observer : observers_)
    observer.OnWillRemoveBookmarks(this, parent, index, node);

  std::set<GURL> removed_urls;
  std::unique_ptr<BookmarkNode> owned_node =
      url_index_->Remove(AsMutable(node), &removed_urls);
  RemoveNodeFromIndexRecursive(owned_node.get());

  if (store_)
    store_->ScheduleSave();

  for (BookmarkModelObserver& observer : observers_)
    observer.BookmarkNodeRemoved(this, parent, index, node, removed_urls);

  undo_delegate()->OnBookmarkNodeRemoved(this, parent, index,
                                         std::move(owned_node));
}

void BookmarkModel::RemoveAllUserBookmarks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::set<GURL> removed_urls;
  struct RemoveNodeData {
    const BookmarkNode* parent;
    int index;
    std::unique_ptr<BookmarkNode> node;
  };
  std::vector<RemoveNodeData> removed_node_data_list;

  for (BookmarkModelObserver& observer : observers_)
    observer.OnWillRemoveAllUserBookmarks(this);

  BeginExtensiveChanges();
  // Skip deleting permanent nodes. Permanent bookmark nodes are the root and
  // its immediate children. For removing all non permanent nodes just remove
  // all children of non-root permanent nodes.
  {
    for (const auto& permanent_node : root_->children()) {
      if (!client_->CanBeEditedByUser(permanent_node.get()))
        continue;

      for (size_t j = permanent_node->children().size(); j > 0; --j) {
        std::unique_ptr<BookmarkNode> node = url_index_->Remove(
            permanent_node->children()[j - 1].get(), &removed_urls);
        RemoveNodeFromIndexRecursive(node.get());
        removed_node_data_list.push_back(
            {permanent_node.get(), j - 1, std::move(node)});
      }
    }
  }
  EndExtensiveChanges();
  if (store_)
    store_->ScheduleSave();

  for (BookmarkModelObserver& observer : observers_)
    observer.BookmarkAllUserNodesRemoved(this, removed_urls);

  BeginGroupedChanges();
  for (auto& removed_node_data : removed_node_data_list) {
    undo_delegate()->OnBookmarkNodeRemoved(this, removed_node_data.parent,
                                           removed_node_data.index,
                                           std::move(removed_node_data.node));
  }
  EndGroupedChanges();
}

void BookmarkModel::Move(const BookmarkNode* node,
                         const BookmarkNode* new_parent,
                         size_t index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded_);
  DCHECK(node);
  DCHECK(IsValidIndex(new_parent, index, true));
  DCHECK(!is_root_node(new_parent));
  DCHECK(!is_permanent_node(node));
  DCHECK(!new_parent->HasAncestor(node));

  const BookmarkNode* old_parent = node->parent();
  size_t old_index = old_parent->GetIndexOf(node);

  if (old_parent == new_parent &&
      (index == old_index || index == old_index + 1)) {
    // Node is already in this position, nothing to do.
    return;
  }

  SetDateFolderModified(new_parent, Time::Now());

  if (old_parent == new_parent && index > old_index)
    index--;

  BookmarkNode* mutable_old_parent = AsMutable(old_parent);
  std::unique_ptr<BookmarkNode> owned_node =
      mutable_old_parent->Remove(old_index);
  BookmarkNode* mutable_new_parent = AsMutable(new_parent);
  mutable_new_parent->Add(std::move(owned_node), index);

  if (store_)
    store_->ScheduleSave();

  for (BookmarkModelObserver& observer : observers_)
    observer.BookmarkNodeMoved(this, old_parent, old_index, new_parent, index);
}

void BookmarkModel::Copy(const BookmarkNode* node,
                         const BookmarkNode* new_parent,
                         size_t index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded_);
  DCHECK(node);
  DCHECK(IsValidIndex(new_parent, index, true));
  DCHECK(!is_root_node(new_parent));
  DCHECK(!is_permanent_node(node));
  DCHECK(!new_parent->HasAncestor(node));

  SetDateFolderModified(new_parent, Time::Now());
  BookmarkNodeData drag_data(node);
  // CloneBookmarkNode will use BookmarkModel methods to do the job, so we
  // don't need to send notifications here.
  CloneBookmarkNode(this, drag_data.elements, new_parent, index, true);

  if (store_)
    store_->ScheduleSave();
}

const gfx::Image& BookmarkModel::GetFavicon(const BookmarkNode* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(node);
  if (node->favicon_state() == BookmarkNode::INVALID_FAVICON) {
    BookmarkNode* mutable_node = AsMutable(node);
    LoadFavicon(mutable_node, client_->PreferTouchIcon()
                                  ? favicon_base::IconType::kTouchIcon
                                  : favicon_base::IconType::kFavicon);
  }
  return node->favicon();
}

favicon_base::IconType BookmarkModel::GetFaviconType(const BookmarkNode* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(node);
  return node->favicon_type();
}

void BookmarkModel::SetTitle(const BookmarkNode* node,
                             const base::string16& title) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(node);

  if (node->GetTitle() == title)
    return;

  if (is_permanent_node(node) && !client_->CanSetPermanentNodeTitle(node)) {
    NOTREACHED();
    return;
  }

  for (BookmarkModelObserver& observer : observers_)
    observer.OnWillChangeBookmarkNode(this, node);

  // The title index doesn't support changing the title, instead we remove then
  // add it back. Only do this for URL nodes. A directory node can have its
  // title changed but should be excluded from the index.
  if (node->is_url())
    titled_url_index_->Remove(node);
  url_index_->SetTitle(AsMutable(node), title);
  if (node->is_url())
    titled_url_index_->Add(node);

  if (store_)
    store_->ScheduleSave();

  for (BookmarkModelObserver& observer : observers_)
    observer.BookmarkNodeChanged(this, node);
}

void BookmarkModel::SetURL(const BookmarkNode* node, const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(node);
  DCHECK(!node->is_folder());

  if (node->url() == url)
    return;

  BookmarkNode* mutable_node = AsMutable(node);
  mutable_node->InvalidateFavicon();
  CancelPendingFaviconLoadRequests(mutable_node);

  for (BookmarkModelObserver& observer : observers_)
    observer.OnWillChangeBookmarkNode(this, node);

  // The title index doesn't support changing the URL, instead we remove then
  // add it back.
  titled_url_index_->Remove(mutable_node);
  url_index_->SetUrl(mutable_node, url);
  titled_url_index_->Add(mutable_node);

  if (store_)
    store_->ScheduleSave();

  for (BookmarkModelObserver& observer : observers_)
    observer.BookmarkNodeChanged(this, node);
}

void BookmarkModel::SetNodeMetaInfo(const BookmarkNode* node,
                                    const std::string& key,
                                    const std::string& value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string old_value;
  if (node->GetMetaInfo(key, &old_value) && old_value == value)
    return;

  for (BookmarkModelObserver& observer : observers_)
    observer.OnWillChangeBookmarkMetaInfo(this, node);

  if (AsMutable(node)->SetMetaInfo(key, value) && store_.get())
    store_->ScheduleSave();

  for (BookmarkModelObserver& observer : observers_)
    observer.BookmarkMetaInfoChanged(this, node);
}

void BookmarkModel::SetNodeMetaInfoMap(
    const BookmarkNode* node,
    const BookmarkNode::MetaInfoMap& meta_info_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const BookmarkNode::MetaInfoMap* old_meta_info_map = node->GetMetaInfoMap();
  if ((!old_meta_info_map && meta_info_map.empty()) ||
      (old_meta_info_map && meta_info_map == *old_meta_info_map))
    return;

  for (BookmarkModelObserver& observer : observers_)
    observer.OnWillChangeBookmarkMetaInfo(this, node);

  AsMutable(node)->SetMetaInfoMap(meta_info_map);
  if (store_)
    store_->ScheduleSave();

  for (BookmarkModelObserver& observer : observers_)
    observer.BookmarkMetaInfoChanged(this, node);
}

void BookmarkModel::DeleteNodeMetaInfo(const BookmarkNode* node,
                                       const std::string& key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const BookmarkNode::MetaInfoMap* meta_info_map = node->GetMetaInfoMap();
  if (!meta_info_map || meta_info_map->find(key) == meta_info_map->end())
    return;

  for (BookmarkModelObserver& observer : observers_)
    observer.OnWillChangeBookmarkMetaInfo(this, node);

  if (AsMutable(node)->DeleteMetaInfo(key) && store_.get())
    store_->ScheduleSave();

  for (BookmarkModelObserver& observer : observers_)
    observer.BookmarkMetaInfoChanged(this, node);
}

void BookmarkModel::AddNonClonedKey(const std::string& key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  non_cloned_keys_.insert(key);
}

void BookmarkModel::OnFaviconsChanged(const std::set<GURL>& page_urls,
                                      const GURL& icon_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!loaded_)
    return;

  std::set<const BookmarkNode*> to_update;
  for (const GURL& page_url : page_urls) {
    std::vector<const BookmarkNode*> nodes;
    GetNodesByURL(page_url, &nodes);
    to_update.insert(nodes.begin(), nodes.end());
  }

  if (!icon_url.is_empty()) {
    // Log Histogram to determine how often |icon_url| is non empty in
    // practice.
    // TODO(pkotwicz): Do something more efficient if |icon_url| is non-empty
    // many times a day for each user.
    UMA_HISTOGRAM_BOOLEAN("Bookmarks.OnFaviconsChangedIconURL", true);

    url_index_->GetNodesWithIconUrl(icon_url, &to_update);
  }

  for (const BookmarkNode* node : to_update) {
    // Rerequest the favicon.
    BookmarkNode* mutable_node = AsMutable(node);
    mutable_node->InvalidateFavicon();
    CancelPendingFaviconLoadRequests(mutable_node);
    for (BookmarkModelObserver& observer : observers_)
      observer.BookmarkNodeFaviconChanged(this, node);
  }
}

void BookmarkModel::SetDateAdded(const BookmarkNode* node, Time date_added) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(node);
  DCHECK(!is_permanent_node(node));

  if (node->date_added() == date_added)
    return;

  AsMutable(node)->set_date_added(date_added);

  // Syncing might result in dates newer than the folder's last modified date.
  if (date_added > node->parent()->date_folder_modified()) {
    // Will trigger store_->ScheduleSave().
    SetDateFolderModified(node->parent(), date_added);
  } else if (store_) {
    store_->ScheduleSave();
  }
}

void BookmarkModel::GetNodesByURL(const GURL& url,
                                  std::vector<const BookmarkNode*>* nodes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (url_index_)
    url_index_->GetNodesByUrl(url, nodes);
}

const BookmarkNode* BookmarkModel::GetMostRecentlyAddedUserNodeForURL(
    const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<const BookmarkNode*> nodes;
  GetNodesByURL(url, &nodes);
  std::sort(nodes.begin(), nodes.end(), &MoreRecentlyAdded);

  // Look for the first node that the user can edit.
  for (size_t i = 0; i < nodes.size(); ++i) {
    if (client_->CanBeEditedByUser(nodes[i]))
      return nodes[i];
  }

  return nullptr;
}

bool BookmarkModel::HasBookmarks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return url_index_ && url_index_->HasBookmarks();
}

bool BookmarkModel::HasNoUserCreatedBookmarksOrFolders() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return bookmark_bar_node_->children().empty() &&
         other_node_->children().empty() && mobile_node_->children().empty();
}

bool BookmarkModel::IsBookmarked(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return url_index_ && url_index_->IsBookmarked(url);
}

void BookmarkModel::GetBookmarks(std::vector<UrlAndTitle>* bookmarks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (url_index_)
    url_index_->GetBookmarks(bookmarks);
}

const BookmarkNode* BookmarkModel::AddFolder(
    const BookmarkNode* parent,
    size_t index,
    const base::string16& title,
    const BookmarkNode::MetaInfoMap* meta_info,
    base::Optional<std::string> guid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded_);
  DCHECK(parent);
  DCHECK(parent->is_folder());
  DCHECK(!is_root_node(parent));
  DCHECK(IsValidIndex(parent, index, true));

  if (guid)
    DCHECK(base::IsValidGUIDOutputString(*guid));
  else
    guid = base::GenerateGUID();

  auto new_node =
      std::make_unique<BookmarkNode>(generate_next_node_id(), *guid, GURL());
  new_node->set_date_folder_modified(Time::Now());
  // Folders shouldn't have line breaks in their titles.
  new_node->SetTitle(title);
  if (meta_info)
    new_node->SetMetaInfoMap(*meta_info);

  return AddNode(AsMutable(parent), index, std::move(new_node));
}

const BookmarkNode* BookmarkModel::AddURL(
    const BookmarkNode* parent,
    size_t index,
    const base::string16& title,
    const GURL& url,
    const BookmarkNode::MetaInfoMap* meta_info,
    base::Optional<base::Time> creation_time,
    base::Optional<std::string> guid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded_);
  DCHECK(url.is_valid());
  DCHECK(parent);
  DCHECK(parent->is_folder());
  DCHECK(!is_root_node(parent));
  DCHECK(IsValidIndex(parent, index, true));

  if (guid)
    DCHECK(base::IsValidGUIDOutputString(*guid));
  else
    guid = base::GenerateGUID();

  if (!creation_time)
    creation_time = Time::Now();

  // Syncing may result in dates newer than the last modified date.
  if (*creation_time > parent->date_folder_modified())
    SetDateFolderModified(parent, *creation_time);

  auto new_node =
      std::make_unique<BookmarkNode>(generate_next_node_id(), *guid, url);
  new_node->SetTitle(title);
  new_node->set_date_added(*creation_time);
  if (meta_info)
    new_node->SetMetaInfoMap(*meta_info);

  return AddNode(AsMutable(parent), index, std::move(new_node));
}

void BookmarkModel::SortChildren(const BookmarkNode* parent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(client_->CanBeEditedByUser(parent));

  if (!parent || !parent->is_folder() || is_root_node(parent) ||
      parent->children().size() <= 1) {
    return;
  }

  for (BookmarkModelObserver& observer : observers_)
    observer.OnWillReorderBookmarkNode(this, parent);

  UErrorCode error = U_ZERO_ERROR;
  std::unique_ptr<icu::Collator> collator(icu::Collator::createInstance(error));
  if (U_FAILURE(error))
    collator.reset(nullptr);
  BookmarkNode* mutable_parent = AsMutable(parent);
  std::sort(mutable_parent->children_.begin(), mutable_parent->children_.end(),
            SortComparator(collator.get()));

  if (store_)
    store_->ScheduleSave();

  for (BookmarkModelObserver& observer : observers_)
    observer.BookmarkNodeChildrenReordered(this, parent);
}

void BookmarkModel::ReorderChildren(
    const BookmarkNode* parent,
    const std::vector<const BookmarkNode*>& ordered_nodes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(client_->CanBeEditedByUser(parent));

  // Ensure that all children in |parent| are in |ordered_nodes|.
  DCHECK_EQ(parent->children().size(), ordered_nodes.size());
  for (const BookmarkNode* node : ordered_nodes)
    DCHECK_EQ(parent, node->parent());

  for (BookmarkModelObserver& observer : observers_)
    observer.OnWillReorderBookmarkNode(this, parent);

  if (ordered_nodes.size() > 1) {
    std::map<const BookmarkNode*, int> order;
    for (size_t i = 0; i < ordered_nodes.size(); ++i)
      order[ordered_nodes[i]] = i;

    std::vector<std::unique_ptr<BookmarkNode>> new_children(
        ordered_nodes.size());
    BookmarkNode* mutable_parent = AsMutable(parent);
    for (auto& child : mutable_parent->children_) {
      size_t new_location = order[child.get()];
      new_children[new_location] = std::move(child);
    }
    mutable_parent->children_.swap(new_children);

    if (store_)
      store_->ScheduleSave();
  }

  for (BookmarkModelObserver& observer : observers_)
    observer.BookmarkNodeChildrenReordered(this, parent);
}

void BookmarkModel::SetDateFolderModified(const BookmarkNode* parent,
                                          const Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(parent);
  AsMutable(parent)->set_date_folder_modified(time);

  if (store_)
    store_->ScheduleSave();
}

void BookmarkModel::ResetDateFolderModified(const BookmarkNode* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetDateFolderModified(node, Time());
}

void BookmarkModel::GetBookmarksMatching(const base::string16& text,
                                         size_t max_count,
                                         std::vector<TitledUrlMatch>* matches) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GetBookmarksMatching(text, max_count,
                       query_parser::MatchingAlgorithm::DEFAULT, matches);
}

void BookmarkModel::GetBookmarksMatching(
    const base::string16& text,
    size_t max_count,
    query_parser::MatchingAlgorithm matching_algorithm,
    std::vector<TitledUrlMatch>* matches) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!loaded_)
    return;

  titled_url_index_->GetResultsMatching(text, max_count, matching_algorithm,
                                        matches);
}

void BookmarkModel::ClearStore() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  store_.reset();
}

void BookmarkModel::RestoreRemovedNode(const BookmarkNode* parent,
                                       size_t index,
                                       std::unique_ptr<BookmarkNode> node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  BookmarkNode* node_ptr = node.get();
  AddNode(AsMutable(parent), index, std::move(node));

  // We might be restoring a folder node that have already contained a set of
  // child nodes. We need to notify all of them.
  NotifyNodeAddedForAllDescendents(node_ptr);
}

void BookmarkModel::NotifyNodeAddedForAllDescendents(const BookmarkNode* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (size_t i = 0; i < node->children().size(); ++i) {
    for (BookmarkModelObserver& observer : observers_)
      observer.BookmarkNodeAdded(this, node, i);
    NotifyNodeAddedForAllDescendents(node->children()[i].get());
  }
}

void BookmarkModel::RemoveNodeFromIndexRecursive(BookmarkNode* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded_);
  DCHECK(!is_permanent_node(node));

  if (node->is_url())
    titled_url_index_->Remove(node);

  // Note that |guid_index_| is used for DCHECK-enabled builds only.
#if DCHECK_IS_ON()
  DCHECK(guid_index_.erase(node->guid()))
      << "Bookmark GUID missing in index: " << node->guid();
#endif  // DCHECK_IS_ON()

  CancelPendingFaviconLoadRequests(node);

  // Recurse through children.
  for (size_t i = node->children().size(); i > 0; --i)
    RemoveNodeFromIndexRecursive(node->children()[i - 1].get());
}

void BookmarkModel::DoneLoading(std::unique_ptr<BookmarkLoadDetails> details) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(details);
  DCHECK(!loaded_);

  next_node_id_ = details->max_id();
  if (details->computed_checksum() != details->stored_checksum() ||
      details->ids_reassigned() || details->guids_reassigned()) {
    // If bookmarks file changed externally, the IDs may have changed
    // externally. In that case, the decoder may have reassigned IDs to make
    // them unique. So when the file has changed externally, we should save the
    // bookmarks file to persist such changes. The same applies if new GUIDs
    // have been assigned to bookmarks.
    if (store_)
      store_->ScheduleSave();
  }

  titled_url_index_ = details->owned_index();
  url_index_ = details->url_index();
  root_ = details->root_node();
  // See declaration for details on why |owned_root_| is reset.
  owned_root_.reset();
  bookmark_bar_node_ = details->bb_node();
  other_node_ = details->other_folder_node();
  mobile_node_ = details->mobile_folder_node();

#if DCHECK_IS_ON()
  AddGuidsToIndexRecursive(root_, &guid_index_);
#endif  // DCHECK_IS_ON()

  titled_url_index_->SetNodeSorter(
      std::make_unique<TypedCountSorter>(client_.get()));
  // Sorting the permanent nodes has to happen on the main thread, so we do it
  // here, after loading completes.
  std::stable_sort(root_->children_.begin(), root_->children_.end(),
                   VisibilityComparator(client_.get()));

  root_->SetMetaInfoMap(details->model_meta_info_map());

  loaded_ = true;
  client_->DecodeBookmarkSyncMetadata(
      details->sync_metadata_str(),
      store_ ? base::BindRepeating(&BookmarkStorage::ScheduleSave,
                                   base::Unretained(store_.get()))
             : base::DoNothing());

  // Notify our direct observers.
  for (BookmarkModelObserver& observer : observers_)
    observer.BookmarkModelLoaded(this, details->ids_reassigned());
}

BookmarkNode* BookmarkModel::AddNode(BookmarkNode* parent,
                                     size_t index,
                                     std::unique_ptr<BookmarkNode> node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  BookmarkNode* node_ptr = node.get();
  url_index_->Add(parent, index, std::move(node));

  if (store_)
    store_->ScheduleSave();

  AddNodeToIndexRecursive(node_ptr);

  for (BookmarkModelObserver& observer : observers_)
    observer.BookmarkNodeAdded(this, parent, index);

  return node_ptr;
}

void BookmarkModel::AddNodeToIndexRecursive(BookmarkNode* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (node->is_url())
    titled_url_index_->Add(node);

  // The node's GUID must be unique. Note that |guid_index_| is used for
  // DCHECK-enabled builds only.
#if DCHECK_IS_ON()
  DCHECK(guid_index_.insert(node->guid()).second)
      << "Duplicate bookmark GUID: " << node->guid();
#endif  // DCHECK_IS_ON()

  for (const auto& child : node->children())
    AddNodeToIndexRecursive(child.get());
}

bool BookmarkModel::IsValidIndex(const BookmarkNode* parent,
                                 size_t index,
                                 bool allow_end) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return parent && parent->is_folder() &&
         (index < parent->children().size() ||
          (allow_end && index == parent->children().size()));
}

void BookmarkModel::OnFaviconDataAvailable(
    BookmarkNode* node,
    favicon_base::IconType icon_type,
    const favicon_base::FaviconImageResult& image_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(node);

  node->set_favicon_load_task_id(base::CancelableTaskTracker::kBadTaskId);
  node->set_favicon_state(BookmarkNode::LOADED_FAVICON);
  if (!image_result.image.IsEmpty()) {
    node->set_favicon_type(icon_type);
    node->set_favicon(image_result.image);
    node->set_icon_url(image_result.icon_url);
    FaviconLoaded(node);
  } else if (icon_type == favicon_base::IconType::kTouchIcon) {
    // Couldn't load the touch icon, fallback to the regular favicon.
    DCHECK(client_->PreferTouchIcon());
    LoadFavicon(node, favicon_base::IconType::kFavicon);
  } else {
    // No favicon available, but we still notify observers.
    FaviconLoaded(node);
  }
}

void BookmarkModel::LoadFavicon(BookmarkNode* node,
                                favicon_base::IconType icon_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (node->is_folder())
    return;

  DCHECK(node->url().is_valid());
  node->set_favicon_state(BookmarkNode::LOADING_FAVICON);
  base::CancelableTaskTracker::TaskId taskId =
      client_->GetFaviconImageForPageURL(
          node->url(), icon_type,
          base::BindOnce(&BookmarkModel::OnFaviconDataAvailable,
                         base::Unretained(this), node, icon_type),
          &cancelable_task_tracker_);
  if (taskId != base::CancelableTaskTracker::kBadTaskId)
    node->set_favicon_load_task_id(taskId);
}

void BookmarkModel::FaviconLoaded(const BookmarkNode* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (BookmarkModelObserver& observer : observers_)
    observer.BookmarkNodeFaviconChanged(this, node);
}

void BookmarkModel::CancelPendingFaviconLoadRequests(BookmarkNode* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (node->favicon_load_task_id() != base::CancelableTaskTracker::kBadTaskId) {
    cancelable_task_tracker_.TryCancel(node->favicon_load_task_id());
    node->set_favicon_load_task_id(base::CancelableTaskTracker::kBadTaskId);
  }
}

int64_t BookmarkModel::generate_next_node_id() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded_);
  return next_node_id_++;
}

void BookmarkModel::SetUndoDelegate(BookmarkUndoDelegate* undo_delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  undo_delegate_ = undo_delegate;
  if (undo_delegate_)
    undo_delegate_->SetUndoProvider(this);
}

BookmarkUndoDelegate* BookmarkModel::undo_delegate() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return undo_delegate_ ? undo_delegate_ : empty_undo_delegate_.get();
}

}  // namespace bookmarks
