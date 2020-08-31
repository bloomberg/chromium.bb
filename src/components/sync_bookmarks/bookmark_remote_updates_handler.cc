// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_remote_updates_handler.h"

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "base/guid.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_piece.h"
#include "base/trace_event/trace_event.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/model/conflict_resolution.h"
#include "components/sync/protocol/unique_position.pb.h"
#include "components/sync_bookmarks/bookmark_specifics_conversions.h"
#include "components/sync_bookmarks/switches.h"

namespace sync_bookmarks {

namespace {

// Used in metrics: "Sync.ProblematicServerSideBookmarks". These values are
// persisted to logs. Entries should not be renumbered and numeric values
// should never be reused.
enum class RemoteBookmarkUpdateError {
  // Remote and local bookmarks types don't match (URL vs. Folder).
  kConflictingTypes = 0,
  // Invalid specifics.
  kInvalidSpecifics = 1,
  // Invalid unique position.
  kInvalidUniquePosition = 2,
  // Permanent node creation in an incremental update.
  kPermanentNodeCreationAfterMerge = 3,
  // Parent entity not found in server.
  kMissingParentEntity = 4,
  // Parent node not found locally.
  kMissingParentNode = 5,
  // Parent entity not found in server when processing a conflict.
  kMissingParentEntityInConflict = 6,
  // Parent Parent node not found locally when processing a conflict.
  kMissingParentNodeInConflict = 7,
  // Failed to create a bookmark.
  kCreationFailure = 8,
  // The bookmark's GUID did not match the originator client item ID.
  kUnexpectedGuid = 9,
  // Parent is not a folder.
  kParentNotFolder = 10,

  kMaxValue = kParentNotFolder,
};

void LogProblematicBookmark(RemoteBookmarkUpdateError problem) {
  base::UmaHistogramEnumeration("Sync.ProblematicServerSideBookmarks", problem);
}

void LogDuplicateBookmarkEntityOnRemoteUpdateCondition(
    BookmarkRemoteUpdatesHandler::DuplicateBookmarkEntityOnRemoteUpdateCondition
        condition) {
  base::UmaHistogramEnumeration(
      "Sync.DuplicateBookmarkEntityOnRemoteUpdateCondition", condition);
}

// Recursive method to traverse a forest created by ReorderUpdates() to to
// emit updates in top-down order. |ordered_updates| must not be null because
// traversed updates are appended to |*ordered_updates|.
void TraverseAndAppendChildren(
    const base::StringPiece& node_id,
    const std::unordered_map<base::StringPiece,
                             const syncer::UpdateResponseData*,
                             base::StringPieceHash>& id_to_updates,
    const std::unordered_map<base::StringPiece,
                             std::vector<base::StringPiece>,
                             base::StringPieceHash>& node_to_children,
    std::vector<const syncer::UpdateResponseData*>* ordered_updates) {
  // If no children to traverse, we are done.
  if (node_to_children.count(node_id) == 0) {
    return;
  }
  // Recurse over all children.
  for (const base::StringPiece& child : node_to_children.at(node_id)) {
    DCHECK_NE(id_to_updates.count(child), 0U);
    ordered_updates->push_back(id_to_updates.at(child));
    TraverseAndAppendChildren(child, id_to_updates, node_to_children,
                              ordered_updates);
  }
}

size_t ComputeChildNodeIndex(const bookmarks::BookmarkNode* parent,
                             const sync_pb::UniquePosition& unique_position,
                             const SyncedBookmarkTracker* bookmark_tracker) {
  // TODO(crbug.com/1084150): remove after investigations are completed.
  TRACE_EVENT0("sync", "ComputeChildNodeIndex");

  const syncer::UniquePosition position =
      syncer::UniquePosition::FromProto(unique_position);
  for (size_t i = 0; i < parent->children().size(); ++i) {
    const bookmarks::BookmarkNode* child = parent->children()[i].get();
    const SyncedBookmarkTracker::Entity* child_entity =
        bookmark_tracker->GetEntityForBookmarkNode(child);
    DCHECK(child_entity);
    const syncer::UniquePosition child_position =
        syncer::UniquePosition::FromProto(
            child_entity->metadata()->unique_position());
    if (position.LessThan(child_position)) {
      return i;
    }
  }
  return parent->children().size();
}

void ApplyRemoteUpdate(
    const syncer::UpdateResponseData& update,
    const SyncedBookmarkTracker::Entity* tracked_entity,
    const SyncedBookmarkTracker::Entity* new_parent_tracked_entity,
    bookmarks::BookmarkModel* model,
    SyncedBookmarkTracker* tracker,
    favicon::FaviconService* favicon_service) {
  const syncer::EntityData& update_entity = update.entity;
  DCHECK(!update_entity.is_deleted());
  DCHECK(tracked_entity);
  DCHECK(new_parent_tracked_entity);
  DCHECK(model);
  DCHECK(tracker);
  DCHECK(favicon_service);
  const bookmarks::BookmarkNode* node = tracked_entity->bookmark_node();
  const bookmarks::BookmarkNode* old_parent = node->parent();
  const bookmarks::BookmarkNode* new_parent =
      new_parent_tracked_entity->bookmark_node();

  if (update_entity.is_folder != node->is_folder()) {
    DLOG(ERROR) << "Could not update node. Remote node is a "
                << (update_entity.is_folder ? "folder" : "bookmark")
                << " while local node is a "
                << (node->is_folder() ? "folder" : "bookmark");
    LogProblematicBookmark(RemoteBookmarkUpdateError::kConflictingTypes);
    return;
  }

  // If there is a different GUID in the specifics and it is valid, we must
  // replace the entire node in order to use it, as GUIDs are immutable. Further
  // updates are then applied to the new node instead.
  if (update_entity.specifics.bookmark().guid() != node->guid() &&
      base::IsValidGUIDOutputString(
          update_entity.specifics.bookmark().guid())) {
    const bookmarks::BookmarkNode* old_node = node;
    node = ReplaceBookmarkNodeGUID(
        node, update_entity.specifics.bookmark().guid(), model);
    tracker->UpdateBookmarkNodePointer(old_node, node);
  }

  UpdateBookmarkNodeFromSpecifics(update_entity.specifics.bookmark(), node,
                                  model, favicon_service);
  // Compute index information before updating the |tracker|.
  const size_t old_index = size_t{old_parent->GetIndexOf(node)};
  const size_t new_index =
      ComputeChildNodeIndex(new_parent, update_entity.unique_position, tracker);
  tracker->Update(tracked_entity, update.response_version,
                  update_entity.modification_time,
                  update_entity.unique_position, update_entity.specifics);

  if (new_parent == old_parent &&
      (new_index == old_index || new_index == old_index + 1)) {
    // Node hasn't moved. No more work to do.
    return;
  }
  // Node has moved to another position under the same parent. Update the model.
  // BookmarkModel takes care of placing the node in the correct position if the
  // node is move to the left. (i.e. no need to subtract one from |new_index|).
  model->Move(node, new_parent, new_index);
}

}  // namespace

BookmarkRemoteUpdatesHandler::BookmarkRemoteUpdatesHandler(
    bookmarks::BookmarkModel* bookmark_model,
    favicon::FaviconService* favicon_service,
    SyncedBookmarkTracker* bookmark_tracker)
    : bookmark_model_(bookmark_model),
      favicon_service_(favicon_service),
      bookmark_tracker_(bookmark_tracker) {
  DCHECK(bookmark_model);
  DCHECK(bookmark_tracker);
  DCHECK(favicon_service);
}

void BookmarkRemoteUpdatesHandler::Process(
    const syncer::UpdateResponseDataList& updates,
    bool got_new_encryption_requirements) {
  TRACE_EVENT0("sync", "BookmarkRemoteUpdatesHandler::Process");

  bookmark_tracker_->CheckAllNodesTracked(bookmark_model_);
  // If new encryption requirements come from the server, the entities that are
  // in |updates| will be recorded here so they can be ignored during the
  // re-encryption phase at the end.
  std::unordered_set<std::string> entities_with_up_to_date_encryption;

  for (const syncer::UpdateResponseData* update : ReorderUpdates(&updates)) {
    const syncer::EntityData& update_entity = update->entity;
    // Only non deletions and non premanent node should have valid specifics and
    // unique positions.
    if (!update_entity.is_deleted() &&
        update_entity.server_defined_unique_tag.empty()) {
      if (!IsValidBookmarkSpecifics(update_entity.specifics.bookmark(),
                                    update_entity.is_folder)) {
        // Ignore updates with invalid specifics.
        DLOG(ERROR)
            << "Couldn't process an update bookmark with an invalid specifics.";
        LogProblematicBookmark(RemoteBookmarkUpdateError::kInvalidSpecifics);
        continue;
      }
      if (!syncer::UniquePosition::FromProto(update_entity.unique_position)
               .IsValid()) {
        // Ignore updates with invalid unique position.
        DLOG(ERROR) << "Couldn't process an update bookmark with an invalid "
                       "unique position.";
        LogProblematicBookmark(
            RemoteBookmarkUpdateError::kInvalidUniquePosition);
        continue;
      }
      if (!HasExpectedBookmarkGuid(update_entity.specifics.bookmark(),
                                   update_entity.originator_cache_guid,
                                   update_entity.originator_client_item_id)) {
        // Ignore updates with an unexpected GUID.
        DLOG(ERROR)
            << "Couldn't process an update bookmark with unexpected GUID: "
            << update_entity.specifics.bookmark().guid();
        LogProblematicBookmark(RemoteBookmarkUpdateError::kUnexpectedGuid);
        continue;
      }
    }

    const SyncedBookmarkTracker::Entity* tracked_entity =
        bookmark_tracker_->GetEntityForSyncId(update_entity.id);

    // This may be a good chance to populate the client tag for the first time.
    // TODO(crbug.com/1032052): Remove this code once all local sync metadata
    // is required to populate the client tag (and be considered invalid
    // otherwise).
    bool local_guid_needs_update = false;
    const std::string& remote_guid = update_entity.specifics.bookmark().guid();
    if (tracked_entity && !update_entity.is_deleted() &&
        update_entity.server_defined_unique_tag.empty() &&
        !tracked_entity->final_guid_matches(remote_guid)) {
      DCHECK(base::IsValidGUIDOutputString(remote_guid));
      bookmark_tracker_->PopulateFinalGuid(tracked_entity, remote_guid);
      // In many cases final_guid_matches() may return false because a final
      // GUID is not known for sure, but actually it matches the local GUID.
      if (tracked_entity->bookmark_node() &&
          remote_guid != tracked_entity->bookmark_node()->guid()) {
        local_guid_needs_update = true;
      }
    }

    if (tracked_entity &&
        tracked_entity->metadata()->server_version() >=
            update->response_version &&
        !local_guid_needs_update) {
      // Seen this update before; just ignore it.
      continue;
    }

    // If a commit succeeds, but the response does not come back fast enough
    // (e.g. before shutdown or crash), then the |bookmark_tracker_| might
    // assume that it was never committed. The server will track the client that
    // sent up the original commit and return this in a get updates response. We
    // need to check if we have an entry that didn't get its server id updated
    // correctly. The server sends down a |originator_cache_guid| and an
    // |original_client_item_id|. If we have a entry by that description, we
    // should update the |sync_id| in |bookmark_tracker_|. The rest of code will
    // handle this a conflict and adjust the model if needed.
    const SyncedBookmarkTracker::Entity* old_tracked_entity =
        bookmark_tracker_->GetEntityForSyncId(
            update_entity.originator_client_item_id);
    if (old_tracked_entity) {
      if (tracked_entity) {
        DCHECK_NE(tracked_entity, old_tracked_entity);
        // We generally shouldn't have an entry for both the old ID and the new
        // ID, but it could happen due to some past bug (see crbug.com/1004205).
        // In that case, the two entries should be duplicates in the sense that
        // they have the same URL.
        // TODO(crbug.com/516866): Clean up the workaround once this has been
        // resolved.
        const bookmarks::BookmarkNode* old_node =
            old_tracked_entity->bookmark_node();
        const bookmarks::BookmarkNode* new_node =
            tracked_entity->bookmark_node();
        if (new_node == nullptr) {
          // This might happen in case a synced bookmark (with a non-temporary
          // server ID but no known client tag) was deleted locally and then
          // recreated locally while the commit is in flight. This leads to two
          // entities in the tracker (for the same bookmark GUID), one of them
          // being a tombstone (|tracked_entity|). The commit response (for the
          // deletion) may never be received (e.g. network issues) and instead a
          // remote update is received (possibly our own reflection). Resolving
          // a situation with duplicate entries is simple as long as at least
          // one of the two (it could also be both) is a tombstone: one of the
          // entities can be simply untracked.
          //
          // In the current case the |old_tracked_entity| must be a new entity
          // since it does not have server ID yet. The |new_node| must be always
          // a tombstone (the bookmark which was deleted). Just remove the
          // tombstone and continue applying current update (even if |old_node|
          // is a tombstone too).
          bookmark_tracker_->Remove(tracked_entity);
          tracked_entity = nullptr;
          LogDuplicateBookmarkEntityOnRemoteUpdateCondition(
              DuplicateBookmarkEntityOnRemoteUpdateCondition::
                  kServerIdTombstone);
        } else {
          // |old_node| may be null when |old_entity| is a tombstone pending
          // commit.
          if (old_node != nullptr) {
            DCHECK_NE(old_node, new_node);
            bookmark_model_->Remove(old_node);
            LogDuplicateBookmarkEntityOnRemoteUpdateCondition(
                DuplicateBookmarkEntityOnRemoteUpdateCondition::
                    kBothEntitiesNonTombstone);
          } else {
            LogDuplicateBookmarkEntityOnRemoteUpdateCondition(
                DuplicateBookmarkEntityOnRemoteUpdateCondition::
                    kTempSyncIdTombstone);
          }
          bookmark_tracker_->Remove(old_tracked_entity);
          continue;
        }
      }

      bookmark_tracker_->UpdateSyncIdForLocalCreationIfNeeded(
          old_tracked_entity,
          /*sync_id=*/update_entity.id);

      // The tracker has changed. Re-retrieve the |tracker_entity|.
      tracked_entity = bookmark_tracker_->GetEntityForSyncId(update_entity.id);
    }

    if (tracked_entity && tracked_entity->IsUnsynced()) {
      ProcessConflict(*update, tracked_entity);
      if (!bookmark_tracker_->GetEntityForSyncId(update_entity.id)) {
        // During conflict resolution, the entity could be dropped in case of
        // a conflict between local and remote deletions. We shouldn't worry
        // about changes to the encryption in that case.
        continue;
      }
    } else if (update_entity.is_deleted()) {
      ProcessDelete(update_entity, tracked_entity);
      // If the local entity has been deleted, no need to check for out of date
      // encryption. Therefore, we can go ahead and process the next update.
      continue;
    } else if (!tracked_entity) {
      tracked_entity = ProcessCreate(*update);
      if (!tracked_entity) {
        // If no new node has been tracked, we shouldn't worry about changes to
        // the encryption.
        continue;
      }
      // TODO(crbug.com/516866): The below CHECK is added to debug some crashes.
      // Should be removed after figuring out the reason for the crash.
      CHECK_EQ(tracked_entity,
               bookmark_tracker_->GetEntityForSyncId(update_entity.id));
    } else {
      // Ignore changes to the permanent nodes (e.g. bookmarks bar). We only
      // care about their children.
      if (bookmark_model_->is_permanent_node(tracked_entity->bookmark_node())) {
        continue;
      }
      ProcessUpdate(*update, tracked_entity);
      // TODO(crbug.com/516866): The below CHECK is added to debug some crashes.
      // Should be removed after figuring out the reason for the crash.
      CHECK_EQ(tracked_entity,
               bookmark_tracker_->GetEntityForSyncId(update_entity.id));
    }
    // If the received entity has out of date encryption, we schedule another
    // commit to fix it.
    if (bookmark_tracker_->model_type_state().encryption_key_name() !=
        update->encryption_key_name) {
      DVLOG(2) << "Bookmarks: Requesting re-encrypt commit "
               << update->encryption_key_name << " -> "
               << bookmark_tracker_->model_type_state().encryption_key_name();
      bookmark_tracker_->IncrementSequenceNumber(tracked_entity);
    }

    if (got_new_encryption_requirements) {
      entities_with_up_to_date_encryption.insert(update_entity.id);
    }
  }

  // Recommit entities with out of date encryption.
  if (got_new_encryption_requirements) {
    std::vector<const SyncedBookmarkTracker::Entity*> all_entities =
        bookmark_tracker_->GetAllEntities();
    for (const SyncedBookmarkTracker::Entity* entity : all_entities) {
      // No need to recommit tombstones and permanent nodes.
      if (entity->metadata()->is_deleted()) {
        continue;
      }
      DCHECK(entity->bookmark_node());
      if (entity->bookmark_node()->is_permanent_node()) {
        continue;
      }
      if (entities_with_up_to_date_encryption.count(
              entity->metadata()->server_id()) != 0) {
        continue;
      }
      bookmark_tracker_->IncrementSequenceNumber(entity);
    }
  }
  bookmark_tracker_->CheckAllNodesTracked(bookmark_model_);
}

// static
std::vector<const syncer::UpdateResponseData*>
BookmarkRemoteUpdatesHandler::ReorderUpdatesForTest(
    const syncer::UpdateResponseDataList* updates) {
  return ReorderUpdates(updates);
}

// static
std::vector<const syncer::UpdateResponseData*>
BookmarkRemoteUpdatesHandler::ReorderUpdates(
    const syncer::UpdateResponseDataList* updates) {
  // This method sorts the remote updates according to the following rules:
  // 1. Creations and updates come before deletions.
  // 2. Parent creation/update should come before child creation/update.
  // 3. No need to further order deletions. Parent deletions can happen before
  //    child deletions. This is safe because all updates (e.g. moves) should
  //    have been processed already.

  // The algorithm works by constructing a forest of all non-deletion updates
  // and then traverses each tree in the forest recursively: Forest
  // Construction:
  // 1. Iterate over all updates and construct the |parent_to_children| map and
  //    collect all parents in |roots|.
  // 2. Iterate over all updates again and drop any parent that has a
  //    coressponding update. What's left in |roots| are the roots of the
  //    forest.
  // 3. Start at each root in |roots|, emit the update and recurse over its
  //    children.

  std::unordered_map<base::StringPiece, const syncer::UpdateResponseData*,
                     base::StringPieceHash>
      id_to_updates;
  std::set<base::StringPiece> roots;
  std::unordered_map<base::StringPiece, std::vector<base::StringPiece>,
                     base::StringPieceHash>
      parent_to_children;

  // Add only non-deletions to |id_to_updates|.
  for (const syncer::UpdateResponseData& update : *updates) {
    const syncer::EntityData& update_entity = update.entity;
    // Ignore updates to root nodes.
    if (update_entity.parent_id == "0") {
      continue;
    }
    if (update_entity.is_deleted()) {
      continue;
    }
    id_to_updates[update_entity.id] = &update;
  }
  // Iterate over |id_to_updates| and construct |roots| and
  // |parent_to_children|.
  for (const std::pair<const base::StringPiece,
                       const syncer::UpdateResponseData*>& pair :
       id_to_updates) {
    const syncer::EntityData& update_entity = pair.second->entity;
    parent_to_children[update_entity.parent_id].push_back(update_entity.id);
    // If this entity's parent has no pending update, add it to |roots|.
    if (id_to_updates.count(update_entity.parent_id) == 0) {
      roots.insert(update_entity.parent_id);
    }
  }
  // |roots| contains only root of all trees in the forest all of which are
  // ready to be processed because none has a pending update.
  std::vector<const syncer::UpdateResponseData*> ordered_updates;
  for (const base::StringPiece& root : roots) {
    TraverseAndAppendChildren(root, id_to_updates, parent_to_children,
                              &ordered_updates);
  }

  int root_node_updates_count = 0;
  // Add deletions.
  for (const syncer::UpdateResponseData& update : *updates) {
    const syncer::EntityData& update_entity = update.entity;
    // Ignore updates to root nodes.
    if (update_entity.parent_id == "0") {
      root_node_updates_count++;
      continue;
    }
    if (update_entity.is_deleted()) {
      ordered_updates.push_back(&update);
    }
  }
  // All non root updates should have been included in |ordered_updates|.
  DCHECK_EQ(updates->size(), ordered_updates.size() + root_node_updates_count);
  return ordered_updates;
}

const SyncedBookmarkTracker::Entity*
BookmarkRemoteUpdatesHandler::ProcessCreate(
    const syncer::UpdateResponseData& update) {
  const syncer::EntityData& update_entity = update.entity;
  DCHECK(!update_entity.is_deleted());
  if (!update_entity.server_defined_unique_tag.empty()) {
    DLOG(ERROR)
        << "Permanent nodes should have been merged during intial sync.";
    LogProblematicBookmark(
        RemoteBookmarkUpdateError::kPermanentNodeCreationAfterMerge);
    return nullptr;
  }

  DCHECK(IsValidBookmarkSpecifics(update_entity.specifics.bookmark(),
                                  update_entity.is_folder));

  const bookmarks::BookmarkNode* parent_node = GetParentNode(update_entity);
  if (!parent_node) {
    // If we cannot find the parent, we can do nothing.
    DLOG(ERROR)
        << "Could not find parent of node being added."
        << " Node title: "
        << update_entity.specifics.bookmark().legacy_canonicalized_title()
        << ", parent id = " << update_entity.parent_id;
    LogProblematicBookmark(RemoteBookmarkUpdateError::kMissingParentNode);
    return nullptr;
  }
  if (!parent_node->is_folder()) {
    DLOG(ERROR)
        << "Parent node is not a folder. Node title: "
        << update_entity.specifics.bookmark().legacy_canonicalized_title()
        << ", parent id: " << update_entity.parent_id;
    LogProblematicBookmark(RemoteBookmarkUpdateError::kParentNotFolder);
    return nullptr;
  }
  const bookmarks::BookmarkNode* bookmark_node =
      CreateBookmarkNodeFromSpecifics(
          update_entity.specifics.bookmark(), parent_node,
          ComputeChildNodeIndex(parent_node, update_entity.unique_position,
                                bookmark_tracker_),
          update_entity.is_folder, bookmark_model_, favicon_service_);
  DCHECK(bookmark_node);
  const SyncedBookmarkTracker::Entity* entity = bookmark_tracker_->Add(
      bookmark_node, update_entity.id, update.response_version,
      update_entity.creation_time, update_entity.unique_position,
      update_entity.specifics);
  ReuploadEntityIfNeeded(update_entity.specifics.bookmark(), entity);
  return entity;
}

void BookmarkRemoteUpdatesHandler::ProcessUpdate(
    const syncer::UpdateResponseData& update,
    const SyncedBookmarkTracker::Entity* tracked_entity) {
  const syncer::EntityData& update_entity = update.entity;
  // Can only update existing nodes.
  DCHECK(tracked_entity);
  DCHECK_EQ(tracked_entity,
            bookmark_tracker_->GetEntityForSyncId(update_entity.id));
  // Must not be a deletion.
  DCHECK(!update_entity.is_deleted());

  DCHECK(IsValidBookmarkSpecifics(update_entity.specifics.bookmark(),
                                  update_entity.is_folder));
  DCHECK(!tracked_entity->IsUnsynced());

  const bookmarks::BookmarkNode* node = tracked_entity->bookmark_node();
  const bookmarks::BookmarkNode* old_parent = node->parent();

  const SyncedBookmarkTracker::Entity* new_parent_entity =
      bookmark_tracker_->GetEntityForSyncId(update_entity.parent_id);
  if (!new_parent_entity) {
    DLOG(ERROR) << "Could not update node. Parent node doesn't exist: "
                << update_entity.parent_id;
    LogProblematicBookmark(RemoteBookmarkUpdateError::kMissingParentEntity);
    return;
  }
  const bookmarks::BookmarkNode* new_parent =
      new_parent_entity->bookmark_node();
  if (!new_parent) {
    DLOG(ERROR)
        << "Could not update node. Parent node has been deleted already.";
    LogProblematicBookmark(RemoteBookmarkUpdateError::kMissingParentNode);
    return;
  }
  if (!new_parent->is_folder()) {
    DLOG(ERROR) << "Could not update node. Parent not is not a folder.";
    LogProblematicBookmark(RemoteBookmarkUpdateError::kParentNotFolder);
    return;
  }
  // Node update could be either in the node data (e.g. title or
  // unique_position), or it could be that the node has moved under another
  // parent without any data change. Should check both the data and the parent
  // to confirm that no updates to the model are needed.
  if (tracked_entity->MatchesDataIgnoringParent(update_entity) &&
      new_parent == old_parent) {
    bookmark_tracker_->Update(tracked_entity, update.response_version,
                              update_entity.modification_time,
                              update_entity.unique_position,
                              update_entity.specifics);
    return;
  }
  ApplyRemoteUpdate(update, tracked_entity, new_parent_entity, bookmark_model_,
                    bookmark_tracker_, favicon_service_);
  ReuploadEntityIfNeeded(update_entity.specifics.bookmark(), tracked_entity);
}

void BookmarkRemoteUpdatesHandler::ProcessDelete(
    const syncer::EntityData& update_entity,
    const SyncedBookmarkTracker::Entity* tracked_entity) {
  DCHECK(update_entity.is_deleted());

  DCHECK_EQ(tracked_entity,
            bookmark_tracker_->GetEntityForSyncId(update_entity.id));

  // Handle corner cases first.
  if (tracked_entity == nullptr) {
    // Process deletion only if the entity is still tracked. It could have
    // been recursively deleted already with an earlier deletion of its
    // parent.
    DVLOG(1) << "Received remote delete for a non-existing item.";
    return;
  }

  const bookmarks::BookmarkNode* node = tracked_entity->bookmark_node();
  // Ignore changes to the permanent top-level nodes.  We only care about
  // their children.
  if (bookmark_model_->is_permanent_node(node)) {
    return;
  }
  // Remove the entities of |node| and its children.
  RemoveEntityAndChildrenFromTracker(node);
  // Remove the node and its children from the model.
  bookmark_model_->Remove(node);
}

void BookmarkRemoteUpdatesHandler::ProcessConflict(
    const syncer::UpdateResponseData& update,
    const SyncedBookmarkTracker::Entity* tracked_entity) {
  const syncer::EntityData& update_entity = update.entity;
  // TODO(crbug.com/516866): Handle the case of conflict as a result of
  // re-encryption request.

  // Can only conflict with existing nodes.
  DCHECK(tracked_entity);
  DCHECK_EQ(tracked_entity,
            bookmark_tracker_->GetEntityForSyncId(update_entity.id));

  if (tracked_entity->metadata()->is_deleted() && update_entity.is_deleted()) {
    // Both have been deleted, delete the corresponding entity from the tracker.
    bookmark_tracker_->Remove(tracked_entity);
    DLOG(WARNING) << "Conflict: CHANGES_MATCH";
    return;
  }

  if (update_entity.is_deleted()) {
    // Only remote has been deleted. Local wins. Record that we received the
    // update from the server but leave the pending commit intact.
    bookmark_tracker_->UpdateServerVersion(tracked_entity,
                                           update.response_version);
    DLOG(WARNING) << "Conflict: USE_LOCAL";
    return;
  }

  if (tracked_entity->metadata()->is_deleted()) {
    // Only local node has been deleted. It should be restored from the server
    // data as a remote creation.
    bookmark_tracker_->Remove(tracked_entity);
    ProcessCreate(update);
    DLOG(WARNING) << "Conflict: USE_REMOTE";
    return;
  }

  // No deletions, there are potentially conflicting updates.
  const bookmarks::BookmarkNode* node = tracked_entity->bookmark_node();
  const bookmarks::BookmarkNode* old_parent = node->parent();

  const SyncedBookmarkTracker::Entity* new_parent_entity =
      bookmark_tracker_->GetEntityForSyncId(update_entity.parent_id);
  // The |new_parent_entity| could be null in some racy conditions.  For
  // example, when a client A moves a node and deletes the old parent and
  // commits, and then updates the node again, and at the same time client B
  // updates before receiving the move updates. The client B update will arrive
  // at client A after the parent entity has been deleted already.
  if (!new_parent_entity) {
    DLOG(ERROR) << "Could not update node. Parent node doesn't exist: "
                << update_entity.parent_id;
    LogProblematicBookmark(
        RemoteBookmarkUpdateError::kMissingParentEntityInConflict);
    return;
  }
  const bookmarks::BookmarkNode* new_parent =
      new_parent_entity->bookmark_node();
  // |new_parent| would be null if the parent has been deleted locally and not
  // committed yet. Deletions are executed recursively, so a parent deletions
  // entails child deletion, and if this child has been updated on another
  // client, this would cause conflict.
  if (!new_parent) {
    DLOG(ERROR)
        << "Could not update node. Parent node has been deleted already.";
    LogProblematicBookmark(
        RemoteBookmarkUpdateError::kMissingParentNodeInConflict);
    return;
  }
  // Either local and remote data match or server wins, and in both cases we
  // should squash any pending commits.
  bookmark_tracker_->AckSequenceNumber(tracked_entity);

  // Node update could be either in the node data (e.g. title or
  // unique_position), or it could be that the node has moved under another
  // parent without any data change. Should check both the data and the parent
  // to confirm that no updates to the model are needed.
  if (tracked_entity->MatchesDataIgnoringParent(update_entity) &&
      new_parent == old_parent) {
    bookmark_tracker_->Update(tracked_entity, update.response_version,
                              update_entity.modification_time,
                              update_entity.unique_position,
                              update_entity.specifics);

    // The changes are identical so there isn't a real conflict.
    DLOG(WARNING) << "Conflict: CHANGES_MATCH";
  } else {
    // Conflict where data don't match and no remote deletion, and hence server
    // wins. Update the model from server data.
    DLOG(WARNING) << "Conflict: USE_REMOTE";
    ApplyRemoteUpdate(update, tracked_entity, new_parent_entity,
                      bookmark_model_, bookmark_tracker_, favicon_service_);
  }
  ReuploadEntityIfNeeded(update_entity.specifics.bookmark(), tracked_entity);
}

void BookmarkRemoteUpdatesHandler::RemoveEntityAndChildrenFromTracker(
    const bookmarks::BookmarkNode* node) {
  const SyncedBookmarkTracker::Entity* entity =
      bookmark_tracker_->GetEntityForBookmarkNode(node);
  DCHECK(entity);
  bookmark_tracker_->Remove(entity);

  for (const auto& child : node->children())
    RemoveEntityAndChildrenFromTracker(child.get());
}

const bookmarks::BookmarkNode* BookmarkRemoteUpdatesHandler::GetParentNode(
    const syncer::EntityData& update_entity) const {
  const SyncedBookmarkTracker::Entity* parent_entity =
      bookmark_tracker_->GetEntityForSyncId(update_entity.parent_id);
  if (!parent_entity) {
    return nullptr;
  }
  return parent_entity->bookmark_node();
}

void BookmarkRemoteUpdatesHandler::ReuploadEntityIfNeeded(
    const sync_pb::BookmarkSpecifics& specifics,
    const SyncedBookmarkTracker::Entity* tracked_entity) {
  if (!IsFullTitleReuploadNeeded(specifics)) {
    return;
  }
  bookmark_tracker_->IncrementSequenceNumber(tracked_entity);
}

}  // namespace sync_bookmarks
