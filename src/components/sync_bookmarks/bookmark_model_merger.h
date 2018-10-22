// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_MERGER_H_
#define COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_MERGER_H_

#include <unordered_map>
#include <vector>

#include "base/macros.h"
#include "components/sync/engine/non_blocking_sync_common.h"

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace favicon {
class FaviconService;
}  // namespace favicon

namespace sync_bookmarks {

class SyncedBookmarkTracker;

// Responsible for merging local and remote bookmark models when bookmark sync
// is enabled for the first time by the user (i.e. no sync metadata exists
// locally), so we need a best-effort merge based on similarity. It implements
// similar logic to that in BookmarkModelAssociator::AssociateModels() to be
// used by the BookmarkModelTypeProcessor().
class BookmarkModelMerger {
 public:
  // |updates|, |bookmark_model|, |favicon_service| and |bookmark_tracker| must
  // not be null and must outlive this object.
  BookmarkModelMerger(const syncer::UpdateResponseDataList* updates,
                      bookmarks::BookmarkModel* bookmark_model,
                      favicon::FaviconService* favicon_service,
                      SyncedBookmarkTracker* bookmark_tracker);

  ~BookmarkModelMerger();

  // Merges the remote bookmark model represented as the |updates| received from
  // the sync server and local bookmark model |bookmark_model|, and updates the
  // model and |bookmark_tracker| (all of which are injected in the constructor)
  // accordingly. On return, there will be a 1:1 mapping between bookmark nodes
  // and metadata entities in the injected tracker.
  void Merge();

 private:
  // Merges a local and a remote subtrees. The input nodes are two equivalent
  // local and remote nodes. This method tries to recursively match their
  // children. It updates the |bookmark_tracker_| accordingly.
  void MergeSubtree(const bookmarks::BookmarkNode* local_node,
                    const syncer::UpdateResponseData* remote_update);

  // Creates a local bookmark node for a |remote_update|. The local node is
  // created under |local_parent| at position |index|. If they the remote node
  // has children, this method recursively creates them as well. It updates the
  // |bookmark_tracker_| accordingly.
  void ProcessRemoteCreation(const syncer::UpdateResponseData* remote_update,
                             const bookmarks::BookmarkNode* local_parent,
                             int index);

  // Creates a server counter-part for the local node at position |index|
  // under |parent|. If the local node has children, corresponding server nodes
  // are created recursively. It updates the |bookmark_tracker_| accordingly and
  // new nodes are marked to be committed.
  void ProcessLocalCreation(const bookmarks::BookmarkNode* parent, int index);

  // Gets the bookmark node corresponding to a permanent folder.
  // |update_entity| must contain server_defined_unique_tag that is used to
  // determine the corresponding permanent node.
  const bookmarks::BookmarkNode* GetPermanentFolder(
      const syncer::EntityData& update_entity) const;

  const syncer::UpdateResponseDataList* const updates_;
  bookmarks::BookmarkModel* const bookmark_model_;
  favicon::FaviconService* const favicon_service_;
  SyncedBookmarkTracker* const bookmark_tracker_;
  // Stores the tree of |updates_| as a map from a remote node to a
  // vector of remote children. It's constructed in the c'tor.
  const std::unordered_map<const syncer::UpdateResponseData*,
                           std::vector<const syncer::UpdateResponseData*>>
      updates_tree_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkModelMerger);
};

}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_MERGER_H_
