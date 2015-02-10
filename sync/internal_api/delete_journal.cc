// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/delete_journal.h"

#include "sync/internal_api/public/base_transaction.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/syncable_base_transaction.h"

namespace syncer {

// static
void DeleteJournal::GetBookmarkDeleteJournals(
    BaseTransaction* trans, BookmarkDeleteJournalList *delete_journal_list) {
  syncer::syncable::EntryKernelSet deleted_entries;
  trans->GetDirectory()->delete_journal()->GetDeleteJournals(
      trans->GetWrappedTrans(), BOOKMARKS, &deleted_entries);
  std::set<int64> undecryptable_journal;
  for (syncer::syncable::EntryKernelSet::const_iterator i =
      deleted_entries.begin(); i != deleted_entries.end(); ++i) {
    delete_journal_list->push_back(BookmarkDeleteJournal());
    delete_journal_list->back().id = (*i)->ref(syncer::syncable::META_HANDLE);
    delete_journal_list->back().is_folder = (*i)->ref(syncer::syncable::IS_DIR);

    const sync_pb::EntitySpecifics& specifics = (*i)->ref(
        syncer::syncable::SPECIFICS);
    if (!specifics.has_encrypted()) {
      delete_journal_list->back().specifics = specifics;
    } else {
      std::string plaintext_data = trans->GetCryptographer()->DecryptToString(
          specifics.encrypted());
      sync_pb::EntitySpecifics unencrypted_data;
      if (plaintext_data.length() == 0 ||
          !unencrypted_data.ParseFromString(plaintext_data)) {
        // Fail to decrypt, Add this delete journal to purge.
        undecryptable_journal.insert(delete_journal_list->back().id);
        delete_journal_list->pop_back();
      } else {
        delete_journal_list->back().specifics = unencrypted_data;
      }
    }
  }

  if (!undecryptable_journal.empty()) {
    trans->GetDirectory()->delete_journal()->PurgeDeleteJournals(
        trans->GetWrappedTrans(), undecryptable_journal);
  }
}

// static
void DeleteJournal::PurgeDeleteJournals(BaseTransaction* trans,
                                        const std::set<int64>& ids) {
  trans->GetDirectory()->delete_journal()->PurgeDeleteJournals(
      trans->GetWrappedTrans(), ids);
}

}  // namespace syncer
