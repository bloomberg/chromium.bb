// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_SYNCABLE_SYNCABLE_COLUMNS_H_
#define SYNC_SYNCABLE_SYNCABLE_COLUMNS_H_

#include "sync/syncable/entry_kernel.h"
#include "sync/syncable/syncable_changes_version.h"

namespace syncer {
namespace syncable {

struct ColumnSpec {
  const char* name;
  const char* spec;
};

// Must be in exact same order as fields in entry_kernel.h.
static const ColumnSpec g_metas_columns[] = {
  //////////////////////////////////////
  // int64s
  {"metahandle", "bigint primary key ON CONFLICT FAIL"},
  {"base_version", "bigint default " CHANGES_VERSION_STRING},
  {"server_version", "bigint default 0"},
  // This is the item ID that we store for the embedding application.
  {"local_external_id", "bigint default 0"},
  {"transaction_version", "bigint default 0"},
  // These timestamps are kept in the same format as that of the
  // protocol (ms since Unix epoch).
  {"mtime", "bigint default 0"},
  {"server_mtime", "bigint default 0"},
  {"ctime", "bigint default 0"},
  {"server_ctime", "bigint default 0"},
  //////////////////////////////////////
  // Ids
  {"id", "varchar(255) default \"r\""},
  {"parent_id", "varchar(255) default \"r\""},
  {"server_parent_id", "varchar(255) default \"r\""},
  //////////////////////////////////////
  // bits
  {"is_unsynced", "bit default 0"},
  {"is_unapplied_update", "bit default 0"},
  {"is_del", "bit default 0"},
  {"is_dir", "bit default 0"},
  {"server_is_dir", "bit default 0"},
  {"server_is_del", "bit default 0"},
  //////////////////////////////////////
  // Strings
  {"non_unique_name", "varchar"},
  {"server_non_unique_name", "varchar(255)"},
  {"unique_server_tag", "varchar"},
  {"unique_client_tag", "varchar"},
  {"unique_bookmark_tag", "varchar"},
  //////////////////////////////////////
  // Blobs (serialized protos).
  {"specifics", "blob"},
  {"server_specifics", "blob"},
  {"base_server_specifics", "blob"},
  //////////////////////////////////////
  // Blobs (positions).
  {"server_unique_position", "blob"},
  {"unique_position", "blob"},
  //////////////////////////////////////
  // AttachmentMetadata is a proto that contains all the metadata associated
  // with an entry's attachments.  Each entry has only one AttachmentMetadata
  // proto.  We store a single proto per entry (as opposed to one for each
  // attachment) because it simplifies the database schema and implementation of
  // DirectoryBackingStore.
  {"attachment_metadata", "blob"},
  {"server_attachment_metadata", "blob"}
};

// At least enforce that there are equal number of column names and fields.
COMPILE_ASSERT(arraysize(g_metas_columns) >= FIELD_COUNT, missing_column_name);
COMPILE_ASSERT(arraysize(g_metas_columns) <= FIELD_COUNT, extra_column_names);

static inline const char* ColumnName(int field) {
  DCHECK(field < BEGIN_TEMPS);
  return g_metas_columns[field].name;
}

}  // namespace syncable
}  // namespace syncer

#endif  // SYNC_SYNCABLE_SYNCABLE_COLUMNS_H_
