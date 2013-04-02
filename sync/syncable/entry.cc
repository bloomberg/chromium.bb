// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/syncable/entry.h"

#include <iomanip>

#include "base/json/string_escape.h"
#include "base/string_util.h"
#include "sync/syncable/blob.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/syncable_base_transaction.h"
#include "sync/syncable/syncable_columns.h"

using std::string;

namespace syncer {
namespace syncable {

Entry::Entry(BaseTransaction* trans, GetById, const Id& id)
    : basetrans_(trans) {
  kernel_ = trans->directory()->GetEntryById(id);
}

Entry::Entry(BaseTransaction* trans, GetByClientTag, const string& tag)
    : basetrans_(trans) {
  kernel_ = trans->directory()->GetEntryByClientTag(tag);
}

Entry::Entry(BaseTransaction* trans, GetByServerTag, const string& tag)
    : basetrans_(trans) {
  kernel_ = trans->directory()->GetEntryByServerTag(tag);
}

Entry::Entry(BaseTransaction* trans, GetByHandle, int64 metahandle)
    : basetrans_(trans) {
  kernel_ = trans->directory()->GetEntryByHandle(metahandle);
}

Directory* Entry::dir() const {
  return basetrans_->directory();
}

DictionaryValue* Entry::ToValue(Cryptographer* cryptographer) const {
  DictionaryValue* entry_info = new DictionaryValue();
  entry_info->SetBoolean("good", good());
  if (good()) {
    entry_info->Set("kernel", kernel_->ToValue(cryptographer));
    entry_info->Set("modelType",
                    ModelTypeToValue(GetModelType()));
    entry_info->SetBoolean("existsOnClientBecauseNameIsNonEmpty",
                           ExistsOnClientBecauseNameIsNonEmpty());
    entry_info->SetBoolean("isRoot", IsRoot());
  }
  return entry_info;
}

const string& Entry::Get(StringField field) const {
  DCHECK(kernel_);
  return kernel_->ref(field);
}

ModelType Entry::GetServerModelType() const {
  ModelType specifics_type = kernel_->GetServerModelType();
  if (specifics_type != UNSPECIFIED)
    return specifics_type;

  // Otherwise, we don't have a server type yet.  That should only happen
  // if the item is an uncommitted locally created item.
  // It's possible we'll need to relax these checks in the future; they're
  // just here for now as a safety measure.
  DCHECK(Get(IS_UNSYNCED));
  DCHECK_EQ(Get(SERVER_VERSION), 0);
  DCHECK(Get(SERVER_IS_DEL));
  // Note: can't enforce !Get(ID).ServerKnows() here because that could
  // actually happen if we hit AttemptReuniteLostCommitResponses.
  return UNSPECIFIED;
}

ModelType Entry::GetModelType() const {
  ModelType specifics_type = GetModelTypeFromSpecifics(Get(SPECIFICS));
  if (specifics_type != UNSPECIFIED)
    return specifics_type;
  if (IsRoot())
    return TOP_LEVEL_FOLDER;
  // Loose check for server-created top-level folders that aren't
  // bound to a particular model type.
  if (!Get(UNIQUE_SERVER_TAG).empty() && Get(IS_DIR))
    return TOP_LEVEL_FOLDER;

  return UNSPECIFIED;
}

Id Entry::GetPredecessorId() const {
  return dir()->GetPredecessorId(kernel_);
}

Id Entry::GetSuccessorId() const {
  return dir()->GetSuccessorId(kernel_);
}

Id Entry::GetFirstChildId() const {
  return dir()->GetFirstChildId(basetrans_, kernel_);
}

bool Entry::ShouldMaintainPosition() const {
  return kernel_->ShouldMaintainPosition();
}

std::ostream& operator<<(std::ostream& s, const Blob& blob) {
  for (Blob::const_iterator i = blob.begin(); i != blob.end(); ++i)
    s << std::hex << std::setw(2)
      << std::setfill('0') << static_cast<unsigned int>(*i);
  return s << std::dec;
}

std::ostream& operator<<(std::ostream& os, const Entry& entry) {
  int i;
  EntryKernel* const kernel = entry.kernel_;
  for (i = BEGIN_FIELDS; i < INT64_FIELDS_END; ++i) {
    os << g_metas_columns[i].name << ": "
       << kernel->ref(static_cast<Int64Field>(i)) << ", ";
  }
  for ( ; i < TIME_FIELDS_END; ++i) {
    os << g_metas_columns[i].name << ": "
       << GetTimeDebugString(kernel->ref(static_cast<TimeField>(i))) << ", ";
  }
  for ( ; i < ID_FIELDS_END; ++i) {
    os << g_metas_columns[i].name << ": "
       << kernel->ref(static_cast<IdField>(i)) << ", ";
  }
  os << "Flags: ";
  for ( ; i < BIT_FIELDS_END; ++i) {
    if (kernel->ref(static_cast<BitField>(i)))
      os << g_metas_columns[i].name << ", ";
  }
  for ( ; i < STRING_FIELDS_END; ++i) {
    const std::string& field = kernel->ref(static_cast<StringField>(i));
    os << g_metas_columns[i].name << ": " << field << ", ";
  }
  for ( ; i < PROTO_FIELDS_END; ++i) {
    std::string escaped_str;
    base::JsonDoubleQuote(
        kernel->ref(static_cast<ProtoField>(i)).SerializeAsString(),
        false,
        &escaped_str);
    os << g_metas_columns[i].name << ": " << escaped_str << ", ";
  }
  for ( ; i < UNIQUE_POSITION_FIELDS_END; ++i) {
    os << g_metas_columns[i].name << ": "
       << kernel->ref(static_cast<UniquePositionField>(i)).ToDebugString()
       << ", ";
  }
  os << "TempFlags: ";
  for ( ; i < BIT_TEMPS_END; ++i) {
    if (kernel->ref(static_cast<BitTemp>(i)))
      os << "#" << i - BIT_TEMPS_BEGIN << ", ";
  }
  return os;
}

}  // namespace syncable
}  // namespace syncer
