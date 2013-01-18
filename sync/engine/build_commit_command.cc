// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/build_commit_command.h"

#include <limits>
#include <set>
#include <string>
#include <vector>

#include "base/string_util.h"
#include "sync/engine/syncer_proto_util.h"
#include "sync/protocol/bookmark_specifics.pb.h"
#include "sync/protocol/sync.pb.h"
#include "sync/sessions/ordered_commit_set.h"
#include "sync/sessions/sync_session.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/mutable_entry.h"
#include "sync/syncable/syncable_changes_version.h"
#include "sync/syncable/syncable_proto_util.h"
#include "sync/syncable/syncable_write_transaction.h"
#include "sync/util/time.h"

// TODO(vishwath): Remove this include after node positions have
// shifted to completely using Ordinals.
// See http://crbug.com/145412 .
#include "sync/internal_api/public/base/node_ordinal.h"

using std::set;
using std::string;
using std::vector;

namespace syncer {

using sessions::SyncSession;
using syncable::Entry;
using syncable::IS_DEL;
using syncable::SERVER_ORDINAL_IN_PARENT;
using syncable::IS_UNAPPLIED_UPDATE;
using syncable::IS_UNSYNCED;
using syncable::Id;
using syncable::MutableEntry;
using syncable::SPECIFICS;

// static
int64 BuildCommitCommand::GetFirstPosition() {
  return std::numeric_limits<int64>::min();
}

// static
int64 BuildCommitCommand::GetLastPosition() {
  return std::numeric_limits<int64>::max();
}

// static
int64 BuildCommitCommand::GetGap() {
  return 1LL << 20;
}

BuildCommitCommand::BuildCommitCommand(
    const sessions::OrderedCommitSet& batch_commit_set,
    sync_pb::ClientToServerMessage* commit_message)
  : batch_commit_set_(batch_commit_set), commit_message_(commit_message) {
}

BuildCommitCommand::~BuildCommitCommand() {}

void BuildCommitCommand::AddExtensionsActivityToMessage(
    SyncSession* session, sync_pb::CommitMessage* message) {
  // We only send ExtensionsActivity to the server if bookmarks are being
  // committed.
  ExtensionsActivityMonitor* monitor = session->context()->extensions_monitor();
  if (batch_commit_set_.HasBookmarkCommitId()) {
    // This isn't perfect, since the set of extensions activity may not
    // correlate exactly with the items being committed.  That's OK as
    // long as we're looking for a rough estimate of extensions activity,
    // not an precise mapping of which commits were triggered by which
    // extension.
    //
    // We will push this list of extensions activity back into the
    // ExtensionsActivityMonitor if this commit fails.  That's why we must keep
    // a copy of these records in the session.
    monitor->GetAndClearRecords(session->mutable_extensions_activity());

    const ExtensionsActivityMonitor::Records& records =
        session->extensions_activity();
    for (ExtensionsActivityMonitor::Records::const_iterator it =
         records.begin();
         it != records.end(); ++it) {
      sync_pb::ChromiumExtensionsActivity* activity_message =
          message->add_extensions_activity();
      activity_message->set_extension_id(it->second.extension_id);
      activity_message->set_bookmark_writes_since_last_commit(
          it->second.bookmark_write_count);
    }
  }
}

void BuildCommitCommand::AddClientConfigParamsToMessage(
    SyncSession* session, sync_pb::CommitMessage* message) {
  const ModelSafeRoutingInfo& routing_info = session->routing_info();
  sync_pb::ClientConfigParams* config_params = message->mutable_config_params();
  for (std::map<ModelType, ModelSafeGroup>::const_iterator iter =
          routing_info.begin(); iter != routing_info.end(); ++iter) {
    int field_number = GetSpecificsFieldNumberFromModelType(iter->first);
    config_params->mutable_enabled_type_ids()->Add(field_number);
  }
}

namespace {
void SetEntrySpecifics(MutableEntry* meta_entry,
                       sync_pb::SyncEntity* sync_entry) {
  // Add the new style extension and the folder bit.
  sync_entry->mutable_specifics()->CopyFrom(meta_entry->Get(SPECIFICS));
  sync_entry->set_folder(meta_entry->Get(syncable::IS_DIR));

  DCHECK_EQ(meta_entry->GetModelType(), GetModelType(*sync_entry));
}
}  // namespace

SyncerError BuildCommitCommand::ExecuteImpl(SyncSession* session) {
  commit_message_->set_share(session->context()->account_name());
  commit_message_->set_message_contents(sync_pb::ClientToServerMessage::COMMIT);

  sync_pb::CommitMessage* commit_message = commit_message_->mutable_commit();
  commit_message->set_cache_guid(
      session->write_transaction()->directory()->cache_guid());
  AddExtensionsActivityToMessage(session, commit_message);
  AddClientConfigParamsToMessage(session, commit_message);

  // Cache previously computed position values.  Because |commit_ids|
  // is already in sibling order, we should always hit this map after
  // the first sibling in a consecutive run of commit items.  The
  // entries in this map are (low, high) values describing the
  // space of positions that are immediate successors of the item
  // whose ID is the map's key.
  std::map<Id, std::pair<int64, int64> > position_map;

  for (size_t i = 0; i < batch_commit_set_.Size(); i++) {
    Id id = batch_commit_set_.GetCommitIdAt(i);
    sync_pb::SyncEntity* sync_entry = commit_message->add_entries();
    sync_entry->set_id_string(SyncableIdToProto(id));
    MutableEntry meta_entry(session->write_transaction(),
                            syncable::GET_BY_ID, id);
    CHECK(meta_entry.good());

    DCHECK(0 != session->routing_info().count(meta_entry.GetModelType()))
        << "Committing change to datatype that's not actively enabled.";

    string name = meta_entry.Get(syncable::NON_UNIQUE_NAME);
    CHECK(!name.empty());  // Make sure this isn't an update.
    TruncateUTF8ToByteSize(name, 255, &name);
    sync_entry->set_name(name);

    // Set the non_unique_name.  If we do, the server ignores
    // the |name| value (using |non_unique_name| instead), and will return
    // in the CommitResponse a unique name if one is generated.
    // We send both because it may aid in logging.
    sync_entry->set_non_unique_name(name);

    if (!meta_entry.Get(syncable::UNIQUE_CLIENT_TAG).empty()) {
      sync_entry->set_client_defined_unique_tag(
          meta_entry.Get(syncable::UNIQUE_CLIENT_TAG));
    }

    // Deleted items with server-unknown parent ids can be a problem so we set
    // the parent to 0. (TODO(sync): Still true in protocol?).
    Id new_parent_id;
    if (meta_entry.Get(syncable::IS_DEL) &&
        !meta_entry.Get(syncable::PARENT_ID).ServerKnows()) {
      new_parent_id = session->write_transaction()->root_id();
    } else {
      new_parent_id = meta_entry.Get(syncable::PARENT_ID);
    }
    sync_entry->set_parent_id_string(SyncableIdToProto(new_parent_id));

    // If our parent has changed, send up the old one so the server
    // can correctly deal with multiple parents.
    // TODO(nick): With the server keeping track of the primary sync parent,
    // it should not be necessary to provide the old_parent_id: the version
    // number should suffice.
    if (new_parent_id != meta_entry.Get(syncable::SERVER_PARENT_ID) &&
        0 != meta_entry.Get(syncable::BASE_VERSION) &&
        syncable::CHANGES_VERSION != meta_entry.Get(syncable::BASE_VERSION)) {
      sync_entry->set_old_parent_id(
          SyncableIdToProto(meta_entry.Get(syncable::SERVER_PARENT_ID)));
    }

    int64 version = meta_entry.Get(syncable::BASE_VERSION);
    if (syncable::CHANGES_VERSION == version || 0 == version) {
      // Undeletions are only supported for items that have a client tag.
      DCHECK(!id.ServerKnows() ||
             !meta_entry.Get(syncable::UNIQUE_CLIENT_TAG).empty())
          << meta_entry;

      // Version 0 means to create or undelete an object.
      sync_entry->set_version(0);
    } else {
      DCHECK(id.ServerKnows()) << meta_entry;
      sync_entry->set_version(meta_entry.Get(syncable::BASE_VERSION));
    }
    sync_entry->set_ctime(TimeToProtoTime(meta_entry.Get(syncable::CTIME)));
    sync_entry->set_mtime(TimeToProtoTime(meta_entry.Get(syncable::MTIME)));

    // Deletion is final on the server, let's move things and then delete them.
    if (meta_entry.Get(IS_DEL)) {
      sync_entry->set_deleted(true);
    } else {
      if (meta_entry.Get(SPECIFICS).has_bookmark()) {
        // Common data in both new and old protocol.
        const Id& prev_id = meta_entry.GetPredecessorId();
        string prev_id_string =
            prev_id.IsRoot() ? string() : prev_id.GetServerId();
        sync_entry->set_insert_after_item_id(prev_id_string);

        // Compute a numeric position based on what we know locally.
        std::pair<int64, int64> position_block(
            GetFirstPosition(), GetLastPosition());
        std::map<Id, std::pair<int64, int64> >::iterator prev_pos =
            position_map.find(prev_id);
        if (prev_pos != position_map.end()) {
          position_block = prev_pos->second;
          position_map.erase(prev_pos);
        } else {
          position_block = std::make_pair(
              FindAnchorPosition(syncable::PREV_ID, meta_entry),
              FindAnchorPosition(syncable::NEXT_ID, meta_entry));
        }
        position_block.first = InterpolatePosition(position_block.first,
                                                   position_block.second);

        position_map[id] = position_block;
        sync_entry->set_position_in_parent(position_block.first);
      }
      SetEntrySpecifics(&meta_entry, sync_entry);
    }
  }

  return SYNCER_OK;
}

int64 BuildCommitCommand::FindAnchorPosition(syncable::IdField direction,
                                             const syncable::Entry& entry) {
  Id next_id = entry.Get(direction);
  while (!next_id.IsRoot()) {
    Entry next_entry(entry.trans(),
                     syncable::GET_BY_ID,
                     next_id);
    if (!next_entry.Get(IS_UNSYNCED) && !next_entry.Get(IS_UNAPPLIED_UPDATE)) {
      return NodeOrdinalToInt64(next_entry.Get(SERVER_ORDINAL_IN_PARENT));
    }
    next_id = next_entry.Get(direction);
  }
  return
      direction == syncable::PREV_ID ?
      GetFirstPosition() : GetLastPosition();
}

int64 BuildCommitCommand::InterpolatePosition(const int64 lo,
                                              const int64 hi) {
  DCHECK_LE(lo, hi);

  // The first item to be added under a parent gets a position of zero.
  if (lo == GetFirstPosition() && hi == GetLastPosition())
    return 0;

  // For small gaps, we do linear interpolation.  For larger gaps,
  // we use an additive offset of |GetGap()|.  We are careful to avoid
  // signed integer overflow.
  uint64 delta = static_cast<uint64>(hi) - static_cast<uint64>(lo);
  if (delta <= static_cast<uint64>(GetGap()*2))
    return lo + (static_cast<int64>(delta) + 7) / 8;  // Interpolate.
  else if (lo == GetFirstPosition())
    return hi - GetGap();  // Extend range just before successor.
  else
    return lo + GetGap();  // Use or extend range just after predecessor.
}


}  // namespace syncer
