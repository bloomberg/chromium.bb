// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_model_merger.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/guid.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/sync/base/hash_util.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/engine/engine_util.h"
#include "components/sync_bookmarks/bookmark_specifics_conversions.h"
#include "components/sync_bookmarks/synced_bookmark_tracker.h"
#include "ui/base/models/tree_node_iterator.h"

using syncer::EntityData;
using syncer::UpdateResponseData;
using syncer::UpdateResponseDataList;

namespace sync_bookmarks {

namespace {

static const size_t kInvalidIndex = -1;

// Maximum number of bytes to allow in a title (must match sync's internal
// limits; see write_node.cc).
const int kTitleLimitBytes = 255;

// The sync protocol identifies top-level entities by means of well-known tags,
// (aka server defined tags) which should not be confused with titles or client
// tags that aren't supported by bookmarks (at the time of writing). Each tag
// corresponds to a singleton instance of a particular top-level node in a
// user's share; the tags are consistent across users. The tags allow us to
// locate the specific folders whose contents we care about synchronizing,
// without having to do a lookup by name or path.  The tags should not be made
// user-visible. For example, the tag "bookmark_bar" represents the permanent
// node for bookmarks bar in Chrome. The tag "other_bookmarks" represents the
// permanent folder Other Bookmarks in Chrome.
//
// It is the responsibility of something upstream (at time of writing, the sync
// server) to create these tagged nodes when initializing sync for the first
// time for a user.  Thus, once the backend finishes initializing, the
// ProfileSyncService can rely on the presence of tagged nodes.
const char kBookmarkBarTag[] = "bookmark_bar";
const char kMobileBookmarksTag[] = "synced_bookmarks";
const char kOtherBookmarksTag[] = "other_bookmarks";

using UpdatesPerParentId = std::unordered_map<base::StringPiece,
                                              syncer::UpdateResponseDataList,
                                              base::StringPieceHash>;

// Gets the bookmark node corresponding to a permanent folder identified by
// |server_defined_unique_tag|. |bookmark_model| must not be null.
const bookmarks::BookmarkNode* GetPermanentFolder(
    const bookmarks::BookmarkModel* bookmark_model,
    const std::string& server_defined_unique_tag) {
  DCHECK(bookmark_model);

  if (server_defined_unique_tag == kBookmarkBarTag) {
    return bookmark_model->bookmark_bar_node();
  }
  if (server_defined_unique_tag == kOtherBookmarksTag) {
    return bookmark_model->other_node();
  }
  if (server_defined_unique_tag == kMobileBookmarksTag) {
    return bookmark_model->mobile_node();
  }

  return nullptr;
}

// Canonicalize |title| similar to legacy client's implementation by truncating
// up to |kTitleLimitBytes| and the appending ' ' in some cases.
std::string CanonicalizeTitle(const std::string& title) {
  std::string canonical_title;
  syncer::SyncAPINameToServerName(title, &canonical_title);
  base::TruncateUTF8ToByteSize(canonical_title, kTitleLimitBytes,
                               &canonical_title);
  return canonical_title;
}

// Heuristic to consider two nodes (local and remote) a match by semantics for
// the purpose of merging. Two folders match by semantics if they have the same
// title, two bookmarks match by semantics if they have the same title and url.
// A folder and a bookmark never match.
bool NodeSemanticsMatch(const bookmarks::BookmarkNode* local_node,
                        const EntityData& remote_node) {
  if (local_node->is_folder() != remote_node.is_folder) {
    return false;
  }
  const sync_pb::BookmarkSpecifics& specifics =
      remote_node.specifics.bookmark();
  const std::string local_title = base::UTF16ToUTF8(local_node->GetTitle());
  const std::string remote_title = specifics.title();
  // Titles match if they are identical or the remote one is the canonical form
  // of the local one. The latter is the case when a legacy client has
  // canonicalized the same local title before committing it. Modern clients
  // don't canonicalize titles anymore.
  if (local_title != remote_title &&
      CanonicalizeTitle(local_title) != remote_title) {
    return false;
  }
  if (remote_node.is_folder) {
    return true;
  }
  return local_node->url() == GURL(specifics.url());
}

// Goes through remote updates to detect duplicate GUIDs (should be extremely
// rare) and resolve them by ignoring (clearing) all occurrences except one,
// which if possible will be the one that also matches the originator client
// item ID.
// TODO(crbug.com/978430): Remove this logic and deprecate proto field.
void ResolveDuplicateRemoteGUIDs(syncer::UpdateResponseDataList* updates) {
  std::set<std::string> known_guids;
  // In a first pass we process |originator_client_item_id| which is more
  // authoritative and cannot run into duplicates.
  for (const std::unique_ptr<UpdateResponseData>& update : *updates) {
    DCHECK(update);
    DCHECK(update->entity);

    // |originator_client_item_id| is empty for permanent nodes.
    if (update->entity->is_deleted() ||
        update->entity->originator_client_item_id.empty()) {
      continue;
    }

    known_guids.insert(update->entity->originator_client_item_id);
  }

  // In a second pass, detect if GUIDs in specifics conflict with each other or
  // with |originator_client_item_id| values processed earlier.
  for (std::unique_ptr<UpdateResponseData>& update : *updates) {
    DCHECK(update);
    DCHECK(update->entity);

    const std::string& guid_in_specifics =
        update->entity->specifics.bookmark().guid();
    if (guid_in_specifics.empty() ||
        guid_in_specifics == update->entity->originator_client_item_id) {
      continue;
    }

    bool success = known_guids.insert(guid_in_specifics).second;
    if (!success) {
      // This GUID conflicts with another one, so let's ignore it for the
      // purpose of merging. This mimics the data produced by old clients,
      // without the GUID being populated.
      update->entity->specifics.mutable_bookmark()->clear_guid();
    }
  }
}

// Groups all valid updates by the server ID of their parent and moves them away
// from |*updates|. |updates| must not be null.
UpdatesPerParentId GroupValidUpdatesByParentId(
    UpdateResponseDataList* updates) {
  UpdatesPerParentId updates_per_parent_id;

  for (std::unique_ptr<UpdateResponseData>& update : *updates) {
    DCHECK(update);
    DCHECK(update->entity);

    const EntityData& update_entity = *update->entity;
    if (update_entity.is_deleted()) {
      continue;
    }
    // No need to associate permanent nodes with their parent (the root node).
    // We start merging from the permanent nodes.
    if (!update_entity.server_defined_unique_tag.empty()) {
      continue;
    }
    if (!syncer::UniquePosition::FromProto(update_entity.unique_position)
             .IsValid()) {
      // Ignore updates with invalid positions.
      DLOG(ERROR) << "Remote update with invalid position: "
                  << update_entity.specifics.bookmark().title();
      continue;
    }
    if (!IsValidBookmarkSpecifics(update_entity.specifics.bookmark(),
                                  update_entity.is_folder)) {
      // Ignore updates with invalid specifics.
      DLOG(ERROR) << "Remote update with invalid specifics";
      continue;
    }

    updates_per_parent_id[update_entity.parent_id].push_back(std::move(update));
  }

  return updates_per_parent_id;
}

}  // namespace

class BookmarkModelMerger::RemoteTreeNode final {
 public:
  // Constructs a tree given |update| as root and recursively all descendants by
  // traversing |*updates_per_parent_id|. |update| and |updates_per_parent_id|
  // must not be null. All updates |*updates_per_parent_id| must represent valid
  // updates. Updates corresponding from descendant nodes are moved away from
  // |*updates_per_parent_id|.
  static RemoteTreeNode BuildTree(
      std::unique_ptr<syncer::UpdateResponseData> update,
      UpdatesPerParentId* updates_per_parent_id);

  ~RemoteTreeNode() = default;

  // Allow moves, useful during construction.
  RemoteTreeNode(RemoteTreeNode&&) = default;
  RemoteTreeNode& operator=(RemoteTreeNode&&) = default;

  const syncer::EntityData& entity() const { return *update_->entity; }
  int64_t response_version() const { return update_->response_version; }

  // Direct children nodes, sorted by ascending unique position. These are
  // guaranteed to be valid updates (e.g. IsValidBookmarkSpecifics()).
  const std::vector<RemoteTreeNode>& children() const { return children_; }

  // Recursively emplaces all GUIDs (this node and descendants) into
  // |*guid_to_remote_node_map|, which must not be null.
  void EmplaceSelfAndDescendantsByGUID(
      std::unordered_map<std::string, const RemoteTreeNode*>*
          guid_to_remote_node_map) const {
    DCHECK(guid_to_remote_node_map);

    const std::string& guid = entity().specifics.bookmark().guid();
    if (!guid.empty()) {
      DCHECK(base::IsValidGUID(guid));

      // Duplicate GUIDs have been sorted out before.
      bool success = guid_to_remote_node_map->emplace(guid, this).second;
      DCHECK(success);
    }

    for (const RemoteTreeNode& child : children_) {
      child.EmplaceSelfAndDescendantsByGUID(guid_to_remote_node_map);
    }
  }

 private:
  static bool UniquePositionLessThan(const RemoteTreeNode& lhs,
                                     const RemoteTreeNode& rhs) {
    const syncer::UniquePosition a_pos =
        syncer::UniquePosition::FromProto(lhs.entity().unique_position);
    const syncer::UniquePosition b_pos =
        syncer::UniquePosition::FromProto(rhs.entity().unique_position);
    return a_pos.LessThan(b_pos);
  }

  RemoteTreeNode() = default;

  std::unique_ptr<syncer::UpdateResponseData> update_;
  std::vector<RemoteTreeNode> children_;
};

// static
BookmarkModelMerger::RemoteTreeNode
BookmarkModelMerger::RemoteTreeNode::BuildTree(
    std::unique_ptr<UpdateResponseData> update,
    UpdatesPerParentId* updates_per_parent_id) {
  DCHECK(updates_per_parent_id);
  DCHECK(update);
  DCHECK(update->entity);

  RemoteTreeNode node;
  node.update_ = std::move(update);

  // Populate descendants recursively.
  for (std::unique_ptr<UpdateResponseData>& child_update :
       (*updates_per_parent_id)[node.entity().id]) {
    DCHECK(child_update);
    DCHECK(child_update->entity);
    DCHECK_EQ(child_update->entity->parent_id, node.entity().id);
    DCHECK(IsValidBookmarkSpecifics(child_update->entity->specifics.bookmark(),
                                    child_update->entity->is_folder));

    node.children_.push_back(
        BuildTree(std::move(child_update), updates_per_parent_id));
  }

  // Sort the children according to their unique position.
  std::stable_sort(node.children_.begin(), node.children_.end(),
                   UniquePositionLessThan);

  return node;
}

BookmarkModelMerger::BookmarkModelMerger(
    UpdateResponseDataList updates,
    bookmarks::BookmarkModel* bookmark_model,
    favicon::FaviconService* favicon_service,
    SyncedBookmarkTracker* bookmark_tracker)
    : bookmark_model_(bookmark_model),
      favicon_service_(favicon_service),
      bookmark_tracker_(bookmark_tracker),
      remote_forest_(BuildRemoteForest(std::move(updates))),
      guid_to_match_map_(
          FindGuidMatchesOrReassignLocal(remote_forest_, bookmark_model_)) {
  DCHECK(bookmark_tracker_->IsEmpty());
  DCHECK(favicon_service);
}

BookmarkModelMerger::~BookmarkModelMerger() {}

void BookmarkModelMerger::Merge() {
  // Algorithm description:
  // Match up the roots and recursively do the following:
  // * For each remote node for the current remote (sync) parent node, either
  //   find a local node with equal GUID anywhere throughout the tree or find
  //   the best matching bookmark node under the corresponding local bookmark
  //   parent node using semantics. If the found node has the same GUID as a
  //   different remote bookmark, we do not consider it a semantics match, as
  //   GUID matching takes precedence. If no matching node is found, create a
  //   new bookmark node in the same position as the corresponding remote node.
  //   If a matching node is found, update the properties of it from the
  //   corresponding remote node.
  // * When all children remote nodes are done, add the extra children bookmark
  //   nodes to the remote (sync) parent node, unless they will be later matched
  //   by GUID.
  //
  // The semantics best match algorithm uses folder title or bookmark title/url
  // to perform the primary match. If there are multiple match candidates it
  // selects the first one.
  // Associate permanent folders.
  for (const auto& tree_tag_and_root : remote_forest_) {
    const bookmarks::BookmarkNode* permanent_folder =
        GetPermanentFolder(bookmark_model_, tree_tag_and_root.first);
    if (!permanent_folder) {
      continue;
    }
    MergeSubtree(/*local_subtree_root=*/permanent_folder,
                 /*remote_node=*/tree_tag_and_root.second);
  }
}

// static
BookmarkModelMerger::RemoteForest BookmarkModelMerger::BuildRemoteForest(
    syncer::UpdateResponseDataList updates) {
  ResolveDuplicateRemoteGUIDs(&updates);

  // Filter out invalid remote updates and group the valid ones by the server ID
  // of their parent.
  UpdatesPerParentId updates_per_parent_id =
      GroupValidUpdatesByParentId(&updates);

  // Construct one tree per permanent entity.
  RemoteForest update_forest;
  for (std::unique_ptr<UpdateResponseData>& update : updates) {
    if (!update || update->entity->server_defined_unique_tag.empty()) {
      continue;
    }

    // Make a copy of the string to avoid relying on argument evaluation order.
    const std::string server_defined_unique_tag =
        update->entity->server_defined_unique_tag;

    update_forest.emplace(
        server_defined_unique_tag,
        RemoteTreeNode::BuildTree(std::move(update), &updates_per_parent_id));
  }

  // TODO(crbug.com/978430): Add UMA to record the number of orphan nodes.

  return update_forest;
}

// static
std::unordered_map<std::string, BookmarkModelMerger::GuidMatch>
BookmarkModelMerger::FindGuidMatchesOrReassignLocal(
    const RemoteForest& remote_forest,
    bookmarks::BookmarkModel* bookmark_model) {
  DCHECK(bookmark_model);

  if (!base::FeatureList::IsEnabled(switches::kMergeBookmarksUsingGUIDs)) {
    return {};
  }

  // Build a temporary lookup table for remote GUIDs.
  std::unordered_map<std::string, const RemoteTreeNode*>
      guid_to_remote_node_map;
  for (const auto& tree_tag_and_root : remote_forest) {
    tree_tag_and_root.second.EmplaceSelfAndDescendantsByGUID(
        &guid_to_remote_node_map);
  }

  // Iterate through all local bookmarks to find matches by GUID.
  std::unordered_map<std::string, BookmarkModelMerger::GuidMatch>
      guid_to_match_map;
  ui::TreeNodeIterator<const bookmarks::BookmarkNode> iterator(
      bookmark_model->root_node());
  while (iterator.has_next()) {
    const bookmarks::BookmarkNode* const node = iterator.Next();
    DCHECK(base::IsValidGUID(node->guid()));

    const auto remote_it = guid_to_remote_node_map.find(node->guid());
    if (remote_it == guid_to_remote_node_map.end()) {
      continue;
    }

    const RemoteTreeNode* const remote_node = remote_it->second;
    const syncer::EntityData& remote_entity = remote_node->entity();

    // Permanent nodes don't match by GUID but by |server_defined_unique_tag|.
    // As extra precaution, specially with remote GUIDs in mind, let's ignore
    // them explicitly here.
    if (node->is_permanent_node() ||
        GetPermanentFolder(bookmark_model,
                           remote_entity.server_defined_unique_tag) !=
            nullptr) {
      continue;
    }

    if (node->is_folder() != remote_entity.is_folder ||
        (node->is_url() &&
         node->url() != remote_entity.specifics.bookmark().url())) {
      // If local node and its remote node match are conflicting in node type or
      // URL, replace local GUID with a random GUID.
      // TODO(crbug.com/978430): Local GUIDs should also be reassigned if they
      // match a remote originator_client_item_id.
      ReplaceBookmarkNodeGUID(node, base::GenerateGUID(), bookmark_model);
      continue;
    }

    guid_to_match_map.emplace(node->guid(), GuidMatch{node, remote_node});
  }

  return guid_to_match_map;
}

void BookmarkModelMerger::MergeSubtree(
    const bookmarks::BookmarkNode* local_subtree_root,
    const RemoteTreeNode& remote_node) {
  const EntityData& remote_update_entity = remote_node.entity();
  bookmark_tracker_->Add(
      remote_update_entity.id, local_subtree_root,
      remote_node.response_version(), remote_update_entity.creation_time,
      remote_update_entity.unique_position, remote_update_entity.specifics);

  // If there are remote child updates, try to match them.
  for (size_t remote_index = 0; remote_index < remote_node.children().size();
       ++remote_index) {
    const RemoteTreeNode& remote_child =
        remote_node.children().at(remote_index);
    const bookmarks::BookmarkNode* matching_local_node =
        FindMatchingLocalNode(remote_child, local_subtree_root, remote_index);
    // If no match found, create a corresponding local node.
    if (!matching_local_node) {
      ProcessRemoteCreation(remote_child, local_subtree_root, remote_index);
      continue;
    }
    DCHECK(!local_subtree_root->HasAncestor(matching_local_node));
    // Move if required, no-op otherwise.
    bookmark_model_->Move(matching_local_node, local_subtree_root,
                          remote_index);
    // Since nodes are matching, their subtrees should be merged as well.
    matching_local_node = UpdateBookmarkNodeFromSpecificsIncludingGUID(
        matching_local_node, remote_child);
    MergeSubtree(matching_local_node, remote_child);
  }

  // At this point all the children of |remote_node| have corresponding local
  // nodes under |local_subtree_root| and they are all in the right positions:
  // from 0 to remote_node.children().size() - 1.
  //
  // This means, the children starting from remote_node.children().size() in
  // the parent bookmark node are the ones that are not present in the parent
  // sync node and not tracked yet. So create all of the remaining local nodes.
  DCHECK_LE(remote_node.children().size(),
            local_subtree_root->children().size());

  for (size_t i = remote_node.children().size();
       i < local_subtree_root->children().size(); ++i) {
    // If local node has been or will be matched by GUID, skip it.
    if (FindMatchingRemoteNodeByGUID(local_subtree_root->children()[i].get())) {
      continue;
    }
    ProcessLocalCreation(local_subtree_root, i);
  }
}

const bookmarks::BookmarkNode* BookmarkModelMerger::FindMatchingLocalNode(
    const RemoteTreeNode& remote_child,
    const bookmarks::BookmarkNode* local_parent,
    size_t local_child_start_index) const {
  // Try to match child by GUID. If we can't, try to match child by semantics.
  const bookmarks::BookmarkNode* matching_local_node_by_guid =
      FindMatchingLocalNodeByGUID(remote_child);
  if (matching_local_node_by_guid) {
    return matching_local_node_by_guid;
  }

  // All local nodes up to |remote_index-1| have processed already. Look for a
  // matching local node starting with the local node at position
  // |local_child_start_index|. FindMatchingChildBySemanticsStartingAt()
  // returns kInvalidIndex in the case where no semantics match was found or
  // the semantics match found is GUID-matchable to a different node.
  const size_t local_index = FindMatchingChildBySemanticsStartingAt(
      /*remote_node=*/remote_child,
      /*local_parent=*/local_parent,
      /*starting_child_index=*/local_child_start_index);
  if (local_index == kInvalidIndex) {
    // If no match found, return.
    return nullptr;
  }

  // The child at |local_index| has matched by semantics, which also means it
  // does not match by GUID to any other remote node.
  const bookmarks::BookmarkNode* matching_local_node_by_semantics =
      local_parent->children()[local_index].get();
  DCHECK(!FindMatchingRemoteNodeByGUID(matching_local_node_by_semantics));
  return matching_local_node_by_semantics;
}

const bookmarks::BookmarkNode*
BookmarkModelMerger::UpdateBookmarkNodeFromSpecificsIncludingGUID(
    const bookmarks::BookmarkNode* local_node,
    const RemoteTreeNode& remote_node) {
  DCHECK(!local_node->is_permanent_node());
  // Ensure bookmarks have the same URL, otherwise they would not have been
  // matched.
  DCHECK(local_node->is_folder() ||
         local_node->url() ==
             GURL(remote_node.entity().specifics.bookmark().url()));
  const EntityData& remote_update_entity = remote_node.entity();
  const sync_pb::BookmarkSpecifics& specifics =
      remote_update_entity.specifics.bookmark();

  // Update the local GUID if necessary for semantic matches (it's obviously not
  // needed for GUID-based matches).
  const bookmarks::BookmarkNode* possibly_replaced_local_node = local_node;
  if (!specifics.guid().empty() && specifics.guid() != local_node->guid()) {
    // If it's a semantic match, neither of the nodes should be involved in any
    // GUID-based match.
    DCHECK(!FindMatchingLocalNodeByGUID(remote_node));
    DCHECK(!FindMatchingRemoteNodeByGUID(local_node));

    possibly_replaced_local_node =
        ReplaceBookmarkNodeGUID(local_node, specifics.guid(), bookmark_model_);

    // Update |guid_to_match_map_| to avoid pointing to a deleted node. This
    // should not be required in practice, because the algorithm processes each
    // GUID once, but let's update nevertheless to avoid future issues.
    const auto it =
        guid_to_match_map_.find(possibly_replaced_local_node->guid());
    if (it != guid_to_match_map_.end() && it->second.local_node == local_node) {
      it->second.local_node = possibly_replaced_local_node;
    }
  }

  // Update all fields, where no-op changes are handled well.
  UpdateBookmarkNodeFromSpecifics(specifics, possibly_replaced_local_node,
                                  bookmark_model_, favicon_service_);

  return possibly_replaced_local_node;
}

void BookmarkModelMerger::ProcessRemoteCreation(
    const RemoteTreeNode& remote_node,
    const bookmarks::BookmarkNode* local_parent,
    size_t index) {
  DCHECK(!FindMatchingLocalNodeByGUID(remote_node));
  const EntityData& remote_update_entity = remote_node.entity();

  // If specifics do not have a valid GUID, create a new one. Legacy clients do
  // not populate GUID field and if the originator_client_item_id is not of
  // valid GUID format to replace it, the field is left blank.
  sync_pb::EntitySpecifics specifics = remote_node.entity().specifics;
  if (!base::IsValidGUID(specifics.bookmark().guid())) {
    specifics.mutable_bookmark()->set_guid(base::GenerateGUID());
  }

  const bookmarks::BookmarkNode* bookmark_node =
      CreateBookmarkNodeFromSpecifics(specifics.bookmark(), local_parent, index,
                                      remote_update_entity.is_folder,
                                      bookmark_model_, favicon_service_);
  if (!bookmark_node) {
    // We ignore bookmarks we can't add.
    DLOG(ERROR) << "Failed to create bookmark node with title "
                << specifics.bookmark().title() << " and url "
                << specifics.bookmark().url();
    return;
  }
  bookmark_tracker_->Add(remote_update_entity.id, bookmark_node,
                         remote_node.response_version(),
                         remote_update_entity.creation_time,
                         remote_update_entity.unique_position, specifics);

  // Recursively, match by GUID or, if not possible, create local node for all
  // child remote nodes.
  int i = 0;
  for (const RemoteTreeNode& remote_child : remote_node.children()) {
    const bookmarks::BookmarkNode* local_child =
        FindMatchingLocalNodeByGUID(remote_child);
    if (!local_child) {
      ProcessRemoteCreation(remote_child, bookmark_node, i++);
      continue;
    }
    bookmark_model_->Move(local_child, bookmark_node, i++);
    local_child =
        UpdateBookmarkNodeFromSpecificsIncludingGUID(local_child, remote_child);
    MergeSubtree(local_child, remote_child);
  }
}

void BookmarkModelMerger::ProcessLocalCreation(
    const bookmarks::BookmarkNode* parent,
    size_t index) {
  const SyncedBookmarkTracker::Entity* parent_entity =
      bookmark_tracker_->GetEntityForBookmarkNode(parent);
  // Since we are merging top down, parent entity must be tracked.
  DCHECK(parent_entity);

  // Similar to the directory implementation here:
  // https://cs.chromium.org/chromium/src/components/sync/syncable/mutable_entry.cc?l=237&gsn=CreateEntryKernel
  // Assign a temp server id for the entity. Will be overridden by the actual
  // server id upon receiving commit response.
  const bookmarks::BookmarkNode* node = parent->children()[index].get();
  DCHECK(!FindMatchingRemoteNodeByGUID(node));
  DCHECK(base::IsValidGUID(node->guid()));
  const std::string sync_id =
      base::FeatureList::IsEnabled(switches::kMergeBookmarksUsingGUIDs)
          ? node->guid()
          : base::GenerateGUID();
  const int64_t server_version = syncer::kUncommittedVersion;
  const base::Time creation_time = base::Time::Now();
  const std::string& suffix = syncer::GenerateSyncableBookmarkHash(
      bookmark_tracker_->model_type_state().cache_guid(), sync_id);
  syncer::UniquePosition pos;
  // Locally created nodes aren't tracked and hence don't have a unique position
  // yet so we need to produce new ones.
  if (index == 0) {
    pos = syncer::UniquePosition::InitialPosition(suffix);
  } else {
    const SyncedBookmarkTracker::Entity* predecessor_entity =
        bookmark_tracker_->GetEntityForBookmarkNode(
            parent->children()[index - 1].get());
    pos = syncer::UniquePosition::After(
        syncer::UniquePosition::FromProto(
            predecessor_entity->metadata()->unique_position()),
        suffix);
  }
  const sync_pb::EntitySpecifics specifics = CreateSpecificsFromBookmarkNode(
      node, bookmark_model_, /*force_favicon_load=*/true);
  bookmark_tracker_->Add(sync_id, node, server_version, creation_time,
                         pos.ToProto(), specifics);
  // Mark the entity that it needs to be committed.
  bookmark_tracker_->IncrementSequenceNumber(sync_id);
  for (size_t i = 0; i < node->children().size(); ++i) {
    // If a local node hasn't matched with any remote entity, its descendants
    // will neither, unless they have been or will be matched by GUID, in which
    // case we skip them for now.
    if (FindMatchingRemoteNodeByGUID(node->children()[i].get())) {
      continue;
    }
    ProcessLocalCreation(/*parent=*/node, i);
  }
}

size_t BookmarkModelMerger::FindMatchingChildBySemanticsStartingAt(
    const RemoteTreeNode& remote_node,
    const bookmarks::BookmarkNode* local_parent,
    size_t starting_child_index) const {
  const auto& children = local_parent->children();
  DCHECK_LE(starting_child_index, children.size());
  const EntityData& remote_entity = remote_node.entity();
  const auto it =
      std::find_if(children.cbegin() + starting_child_index, children.cend(),
                   [this, &remote_entity](const auto& child) {
                     return NodeSemanticsMatch(child.get(), remote_entity) &&
                            !FindMatchingRemoteNodeByGUID(child.get());
                   });
  return (it == children.cend()) ? kInvalidIndex : (it - children.cbegin());
}

const BookmarkModelMerger::RemoteTreeNode*
BookmarkModelMerger::FindMatchingRemoteNodeByGUID(
    const bookmarks::BookmarkNode* local_node) const {
  DCHECK(local_node);

  const auto it = guid_to_match_map_.find(local_node->guid());
  if (it == guid_to_match_map_.end()) {
    return nullptr;
  }

  DCHECK_EQ(it->second.local_node, local_node);
  return it->second.remote_node;
}

const bookmarks::BookmarkNode* BookmarkModelMerger::FindMatchingLocalNodeByGUID(
    const RemoteTreeNode& remote_node) const {
  const syncer::EntityData& remote_entity = remote_node.entity();
  const auto it =
      guid_to_match_map_.find(remote_entity.specifics.bookmark().guid());
  if (it == guid_to_match_map_.end()) {
    return nullptr;
  }

  DCHECK_EQ(it->second.remote_node, &remote_node);
  return it->second.local_node;
}

}  // namespace sync_bookmarks
