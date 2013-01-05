// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/syncable/delete_journal.h"

#include "base/stl_util.h"
#include "sync/internal_api/public/base/model_type.h"

namespace syncer {
namespace syncable {

DeleteJournal::DeleteJournal(JournalIndex* initial_journal) {
  CHECK(initial_journal);
  delete_journals_.swap(*initial_journal);
}

DeleteJournal::~DeleteJournal() {
  STLDeleteElements(&delete_journals_);
}

size_t DeleteJournal::GetDeleteJournalSize(BaseTransaction* trans) const {
  DCHECK(trans);
  return delete_journals_.size();
}

void DeleteJournal::UpdateDeleteJournalForServerDelete(
    BaseTransaction* trans, bool was_deleted, const EntryKernel& entry) {
  DCHECK(trans);

  // Should be sufficient to check server type only but check for local
  // type too because of incomplete test setup.
  if (!(IsDeleteJournalEnabled(entry.GetServerModelType()) ||
      IsDeleteJournalEnabled(
          GetModelTypeFromSpecifics(entry.ref(SPECIFICS))))) {
    return;
  }

  JournalIndex::iterator it = delete_journals_.find(&entry);

  if (entry.ref(SERVER_IS_DEL)) {
    if (it == delete_journals_.end()) {
      // New delete.
      EntryKernel* t = new EntryKernel(entry);
      delete_journals_.insert(t);
      delete_journals_to_purge_.erase(t->ref(META_HANDLE));
    }
  } else {
    // Undelete. This could happen in two cases:
    // * An entry was deleted then undeleted, i.e. server delete was
    //   overwritten because of entry has unsynced data locally.
    // * A data type was broken, i.e. encountered unrecoverable error, in last
    //   sync session and all its entries were duplicated in delete journals.
    //   On restart, entries are recreated from downloads and recreation calls
    //   UpdateDeleteJournals() to remove live entries from delete journals,
    //   thus only deleted entries remain in journals.
    if (it != delete_journals_.end()) {
      delete_journals_to_purge_.insert((*it)->ref(META_HANDLE));
      delete *it;
      delete_journals_.erase(it);
    } else if (was_deleted) {
      delete_journals_to_purge_.insert(entry.ref(META_HANDLE));
    }
  }
}

void DeleteJournal::GetDeleteJournals(BaseTransaction* trans,
                                      ModelType type,
                                      EntryKernelSet* deleted_entries) {
  DCHECK(trans);
  DCHECK(!passive_delete_journal_types_.Has(type));
  for (JournalIndex::const_iterator it = delete_journals_.begin();
      it != delete_journals_.end(); ++it) {
    if ((*it)->GetServerModelType() == type ||
        GetModelTypeFromSpecifics((*it)->ref(SPECIFICS)) == type) {
      deleted_entries->insert(*it);
    }
  }
  passive_delete_journal_types_.Put(type);
}

void DeleteJournal::PurgeDeleteJournals(BaseTransaction* trans,
                                        const MetahandleSet& to_purge) {
  DCHECK(trans);
  JournalIndex::iterator it = delete_journals_.begin();
  while (it != delete_journals_.end()) {
    int64 handle = (*it)->ref(META_HANDLE);
    if (to_purge.count(handle)) {
      delete *it;
      delete_journals_.erase(it++);
    } else {
      ++it;
    }
  }
  delete_journals_to_purge_.insert(to_purge.begin(), to_purge.end());
}

void DeleteJournal::TakeSnapshotAndClear(BaseTransaction* trans,
                                         EntryKernelSet* journal_entries,
                                         MetahandleSet* journals_to_purge) {
  DCHECK(trans);
  // Move passive delete journals to snapshot. Will copy back if snapshot fails
  // to save.
  JournalIndex::iterator it = delete_journals_.begin();
  while (it != delete_journals_.end()) {
    if (passive_delete_journal_types_.Has((*it)->GetServerModelType()) ||
        passive_delete_journal_types_.Has(GetModelTypeFromSpecifics(
            (*it)->ref(SPECIFICS)))) {
      journal_entries->insert(*it);
      delete_journals_.erase(it++);
    } else {
      ++it;
    }
  }
  *journals_to_purge = delete_journals_to_purge_;
  delete_journals_to_purge_.clear();
}

void DeleteJournal::AddJournalBatch(BaseTransaction* trans,
                                    const EntryKernelSet& entries) {
  DCHECK(trans);
  EntryKernel needle;
  for (EntryKernelSet::const_iterator i = entries.begin();
       i != entries.end(); ++i) {
    needle.put(ID, (*i)->ref(ID));
    if (delete_journals_.find(&needle) == delete_journals_.end()) {
      delete_journals_.insert(new EntryKernel(**i));
    }
    delete_journals_to_purge_.erase((*i)->ref(META_HANDLE));
  }
}

/* static */
bool DeleteJournal::IsDeleteJournalEnabled(ModelType type) {
  switch (type) {
    case BOOKMARKS:
      return true;
    default:
      return false;
  }
}

}  // namespace syncable
}  // namespace syncer
