// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_MERGER_H_
#define COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_MERGER_H_

#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/macros.h"
#include "components/sync/base/unique_position.h"
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
  // |bookmark_model|, |favicon_service| and |bookmark_tracker| must not be
  // null and must outlive this object.
  BookmarkModelMerger(syncer::UpdateResponseDataList updates,
                      bookmarks::BookmarkModel* bookmark_model,
                      favicon::FaviconService* favicon_service,
                      SyncedBookmarkTracker* bookmark_tracker);

  ~BookmarkModelMerger();

  // Merges the remote bookmark model represented as the updates received from
  // the sync server and local bookmark model |bookmark_model|, and updates the
  // model and |bookmark_tracker| (all of which are injected in the constructor)
  // accordingly. On return, there will be a 1:1 mapping between bookmark nodes
  // and metadata entities in the injected tracker.
  void Merge();

 private:
  // Internal representation of a remote tree, composed of nodes.
  class RemoteTreeNode final {
   private:
    using UpdatesPerParentId =
        std::unordered_map<std::string, std::list<syncer::UpdateResponseData>>;

   public:
    // Constructs a tree given |update| as root and recursively all descendants
    // by traversing |*updates_per_parent_id|. |update| and
    // |updates_per_parent_id| must not be null. All updates
    // |*updates_per_parent_id| must represent valid updates. Updates
    // corresponding from descendant nodes are moved away from
    // |*updates_per_parent_id|.
    static RemoteTreeNode BuildTree(syncer::UpdateResponseData update,
                                    UpdatesPerParentId* updates_per_parent_id);

    ~RemoteTreeNode();

    // Allow moves, useful during construction.
    RemoteTreeNode(RemoteTreeNode&&);
    RemoteTreeNode& operator=(RemoteTreeNode&&);

    const syncer::EntityData& entity() const { return update_.entity; }
    int64_t response_version() const { return update_.response_version; }

    // Direct children nodes, sorted by ascending unique position. These are
    // guaranteed to be valid updates (e.g. IsValidBookmarkSpecifics()).
    const std::vector<RemoteTreeNode>& children() const { return children_; }

    // Recursively emplaces all GUIDs (this node and descendants) into
    // |*guid_to_remote_node_map|, which must not be null.
    void EmplaceSelfAndDescendantsByGUID(
        std::unordered_map<std::string, const RemoteTreeNode*>*
            guid_to_remote_node_map) const;

   private:
    static bool UniquePositionLessThan(const RemoteTreeNode& lhs,
                                       const RemoteTreeNode& rhs);

    RemoteTreeNode();

    syncer::UpdateResponseData update_;
    std::vector<RemoteTreeNode> children_;
  };

  // A forest composed of multiple trees where the root of each tree represents
  // a permanent node, keyed by server-defined unique tag of the root.
  using RemoteForest = std::unordered_map<std::string, RemoteTreeNode>;

  // Represents a pair of bookmarks, one local and one remote, that have been
  // matched by GUID. They are guaranteed to have the same type and URL (if
  // applicable).
  struct GuidMatch {
    const bookmarks::BookmarkNode* local_node;
    const RemoteTreeNode* remote_node;
  };

  // Constructs the remote bookmark tree to be merged. Each entry in the
  // returned map is a permanent node, identified (keyed) by the server-defined
  // tag. All invalid updates are filtered out, including invalid bookmark
  // specifics as well as tombstones, in the unlikely event that the server
  // sends tombstones as part of the initial download.
  static RemoteForest BuildRemoteForest(syncer::UpdateResponseDataList updates);

  // Computes bookmark pairs that should be matched by GUID. Local bookmark
  // GUIDs may be regenerated for the case where they collide with a remote GUID
  // that is not compatible (e.g. folder vs non-folder).
  static std::unordered_map<std::string, GuidMatch>
  FindGuidMatchesOrReassignLocal(const RemoteForest& remote_forest,
                                 bookmarks::BookmarkModel* bookmark_model);

  // Merges a local and a remote subtrees. The input nodes are two equivalent
  // local and remote nodes. This method tries to recursively match their
  // children. It updates the |bookmark_tracker_| accordingly.
  void MergeSubtree(const bookmarks::BookmarkNode* local_node,
                    const RemoteTreeNode& remote_node);

  // Updates |local_node| to hold same GUID and semantics as its |remote_node|
  // match. The input nodes are two equivalent local and remote bookmarks that
  // are about to be merged. The output node is the potentially replaced
  // |local_node|. |local_node| must not be a BookmarkPermanentNode.
  const bookmarks::BookmarkNode* UpdateBookmarkNodeFromSpecificsIncludingGUID(
      const bookmarks::BookmarkNode* local_node,
      const RemoteTreeNode& remote_node);

  // Creates a local bookmark node for a |remote_node|. The local node is
  // created under |local_parent| at position |index|. If the remote node has
  // children, this method recursively creates them as well. It updates the
  // |bookmark_tracker_| accordingly.
  void ProcessRemoteCreation(const RemoteTreeNode& remote_node,
                             const bookmarks::BookmarkNode* local_parent,
                             size_t index);

  // Creates a server counter-part for the local node at position |index|
  // under |parent|. If the local node has children, corresponding server nodes
  // are created recursively. It updates the |bookmark_tracker_| accordingly and
  // new nodes are marked to be committed.
  void ProcessLocalCreation(const bookmarks::BookmarkNode* parent,
                            size_t index);

  // Looks for a local node under |local_parent| that matches |remote_node|,
  // starting at index |local_child_start_index|. First attempts to find a match
  // by GUID and otherwise attempts to find one by semantics. If no match is
  // found, a nullptr is returned.
  const bookmarks::BookmarkNode* FindMatchingLocalNode(
      const RemoteTreeNode& remote_node,
      const bookmarks::BookmarkNode* local_parent,
      size_t local_child_start_index) const;

  // If |local_node| has a remote counterpart of the same GUID, returns the
  // corresponding remote node, otherwise returns a nullptr. |local_node| must
  // not be null.
  const RemoteTreeNode* FindMatchingRemoteNodeByGUID(
      const bookmarks::BookmarkNode* local_node) const;

  // If |remote_node| has a local counterpart of the same GUID, returns the
  // corresponding local node, otherwise returns a nullptr.
  const bookmarks::BookmarkNode* FindMatchingLocalNodeByGUID(
      const RemoteTreeNode& remote_node) const;

  // Tries to find a child local node under |local_parent| that matches
  // |remote_node| semantically and returns the index of that node, as long as
  // this local child cannot be matched by GUID to a different node. Matching is
  // decided using NodeSemanticsMatch(). It searches in the children list
  // starting from position |search_starting_child_index|. In case of no match
  // is found, it returns |kInvalidIndex|.
  size_t FindMatchingChildBySemanticsStartingAt(
      const RemoteTreeNode& remote_node,
      const bookmarks::BookmarkNode* local_parent,
      size_t starting_child_index) const;

  // Used to generate a unique position for the current locally created
  // bookmark.
  syncer::UniquePosition GenerateUniquePositionForLocalCreation(
      const bookmarks::BookmarkNode* parent,
      size_t index,
      const std::string& suffix) const;

  bookmarks::BookmarkModel* const bookmark_model_;
  favicon::FaviconService* const favicon_service_;
  SyncedBookmarkTracker* const bookmark_tracker_;
  // Preprocessed remote nodes in the form a forest where each tree's root is a
  // permanent node. Computed upon construction via BuildRemoteForest().
  const RemoteForest remote_forest_;
  std::unordered_map<std::string, GuidMatch> guid_to_match_map_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkModelMerger);
};

}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_MERGER_H_
