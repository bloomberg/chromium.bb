// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_DELETE_JOURNAL_H_
#define SYNC_INTERNAL_API_PUBLIC_DELETE_JOURNAL_H_

#include <vector>

#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/protocol/sync.pb.h"

namespace syncer {

class BaseTransaction;

struct BookmarkDeleteJournal {
  int64 id;   // Metahandle of delete journal entry.
  bool is_folder;
  sync_pb::EntitySpecifics specifics;
};
typedef std::vector<BookmarkDeleteJournal> BookmarkDeleteJournalList;

// Static APIs for passing delete journals between syncer::syncable namspace
// and syncer namespace.
class SYNC_EXPORT DeleteJournal {
 public:
  // Return info about deleted bookmark entries stored in the delete journal
  // of |trans|'s directory.
  // TODO(haitaol): remove this after SyncData supports bookmarks and
  //                all types of delete journals can be returned as
  //                SyncDataList.
  static void GetBookmarkDeleteJournals(
      BaseTransaction* trans, BookmarkDeleteJournalList *delete_journals);

  // Purge delete journals of given IDs from |trans|'s directory.
  static void PurgeDeleteJournals(BaseTransaction* trans,
                                  const std::set<int64>& ids);
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_DELETE_JOURNAL_H_
