// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/commit.h"

#include "base/debug/trace_event.h"
#include "sync/engine/build_commit_command.h"
#include "sync/engine/get_commit_ids_command.h"
#include "sync/engine/process_commit_response_command.h"
#include "sync/engine/syncer_proto_util.h"
#include "sync/sessions/sync_session.h"

using syncable::SYNCER;
using syncable::WriteTransaction;

namespace browser_sync {

using sessions::SyncSession;
using sessions::StatusController;

// Helper function that finds sync items that are ready to be committed to the
// server and serializes them into a commit message protobuf.  It will return
// false iff there are no entries ready to be committed at this time.
//
// The OrderedCommitSet parameter is an output parameter which will contain
// the set of all items which are to be committed.  The number of items in
// the set shall not exceed the maximum batch size.  (The default batch size
// is currently 25, though it can be overwritten by the server.)
//
// The ClientToServerMessage parameter is an output parameter which will contain
// the commit message which should be sent to the server.  It is valid iff the
// return value of this function is true.
bool PrepareCommitMessage(sessions::SyncSession* session,
                          sessions::OrderedCommitSet* commit_set,
                          ClientToServerMessage* commit_message) {
  TRACE_EVENT0("sync", "PrepareCommitMessage");

  commit_set->Clear();
  commit_message->Clear();

  WriteTransaction trans(FROM_HERE, SYNCER, session->context()->directory());
  sessions::ScopedSetSessionWriteTransaction set_trans(session, &trans);

  // Fetch the items to commit.
  const size_t batch_size = session->context()->max_commit_batch_size();
  GetCommitIdsCommand get_commit_ids_command(batch_size, commit_set);
  get_commit_ids_command.Execute(session);

  DVLOG(1) << "Commit message will contain " << commit_set->Size() << " items.";
  if (commit_set->Empty())
    return false;

  // Serialize the message.
  BuildCommitCommand build_commit_command(*commit_set, commit_message);
  build_commit_command.Execute(session);

  return true;
}

SyncerError BuildAndPostCommits(Syncer* syncer,
                                sessions::SyncSession* session) {
  StatusController* status_controller = session->mutable_status_controller();

  sessions::OrderedCommitSet commit_set(session->routing_info());
  ClientToServerMessage commit_message;
  while (PrepareCommitMessage(session, &commit_set, &commit_message)
         && !syncer->ExitRequested()) {
    ClientToServerResponse commit_response;

    DVLOG(1) << "Sending commit message.";
    TRACE_EVENT_BEGIN0("sync", "PostCommit");
    status_controller->set_last_post_commit_result(
        SyncerProtoUtil::PostClientToServerMessage(commit_message,
                                                   &commit_response,
                                                   session));
    TRACE_EVENT_END0("sync", "PostCommit");

    // ProcessCommitResponse includes some code that cleans up after a failure
    // to post a commit message, so we must run it regardless of whether or not
    // the commit succeeds.

    TRACE_EVENT_BEGIN0("sync", "ProcessCommitResponse");
    ProcessCommitResponseCommand process_response_command(
        commit_set, commit_message, commit_response);
    status_controller->set_last_process_commit_response_result(
            process_response_command.Execute(session));
    TRACE_EVENT_END0("sync", "ProcessCommitResponse");

    // Exit early if either the commit or the response processing failed.
    if (status_controller->last_post_commit_result() != SYNCER_OK)
      return status_controller->last_post_commit_result();
    if (status_controller->last_process_commit_response_result() != SYNCER_OK)
      return status_controller->last_process_commit_response_result();
  }

  return SYNCER_OK;
}

}  // namespace browser_sync
