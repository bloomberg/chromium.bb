// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/syncable/directory_backing_store.h"

#include "build/build_config.h"

#include <limits>

#include "base/base64.h"
#include "base/debug/trace_event.h"
#include "base/file_util.h"
#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "sync/protocol/bookmark_specifics.pb.h"
#include "sync/protocol/sync.pb.h"
#include "sync/syncable/syncable-inl.h"
#include "sync/syncable/syncable_columns.h"
#include "sync/util/time.h"

using std::string;

namespace syncer {
namespace syncable {

// This just has to be big enough to hold an UPDATE or INSERT statement that
// modifies all the columns in the entry table.
static const string::size_type kUpdateStatementBufferSize = 2048;

// Increment this version whenever updating DB tables.
extern const int32 kCurrentDBVersion;  // Global visibility for our unittest.
const int32 kCurrentDBVersion = 80;

// Iterate over the fields of |entry| and bind each to |statement| for
// updating.  Returns the number of args bound.
void BindFields(const EntryKernel& entry,
                sql::Statement* statement) {
  int index = 0;
  int i = 0;
  for (i = BEGIN_FIELDS; i < INT64_FIELDS_END; ++i) {
    statement->BindInt64(index++, entry.ref(static_cast<Int64Field>(i)));
  }
  for ( ; i < TIME_FIELDS_END; ++i) {
    statement->BindInt64(index++,
                         TimeToProtoTime(
                             entry.ref(static_cast<TimeField>(i))));
  }
  for ( ; i < ID_FIELDS_END; ++i) {
    statement->BindString(index++, entry.ref(static_cast<IdField>(i)).s_);
  }
  for ( ; i < BIT_FIELDS_END; ++i) {
    statement->BindInt(index++, entry.ref(static_cast<BitField>(i)));
  }
  for ( ; i < STRING_FIELDS_END; ++i) {
    statement->BindString(index++, entry.ref(static_cast<StringField>(i)));
  }
  std::string temp;
  for ( ; i < PROTO_FIELDS_END; ++i) {
    entry.ref(static_cast<ProtoField>(i)).SerializeToString(&temp);
    statement->BindBlob(index++, temp.data(), temp.length());
  }
}

// The caller owns the returned EntryKernel*.  Assumes the statement currently
// points to a valid row in the metas table.
EntryKernel* UnpackEntry(sql::Statement* statement) {
  EntryKernel* kernel = new EntryKernel();
  DCHECK_EQ(statement->ColumnCount(), static_cast<int>(FIELD_COUNT));
  int i = 0;
  for (i = BEGIN_FIELDS; i < INT64_FIELDS_END; ++i) {
    kernel->put(static_cast<Int64Field>(i), statement->ColumnInt64(i));
  }
  for ( ; i < TIME_FIELDS_END; ++i) {
    kernel->put(static_cast<TimeField>(i),
                ProtoTimeToTime(statement->ColumnInt64(i)));
  }
  for ( ; i < ID_FIELDS_END; ++i) {
    kernel->mutable_ref(static_cast<IdField>(i)).s_ =
        statement->ColumnString(i);
  }
  for ( ; i < BIT_FIELDS_END; ++i) {
    kernel->put(static_cast<BitField>(i), (0 != statement->ColumnInt(i)));
  }
  for ( ; i < STRING_FIELDS_END; ++i) {
    kernel->put(static_cast<StringField>(i),
                statement->ColumnString(i));
  }
  for ( ; i < PROTO_FIELDS_END; ++i) {
    kernel->mutable_ref(static_cast<ProtoField>(i)).ParseFromArray(
        statement->ColumnBlob(i), statement->ColumnByteLength(i));
  }
  return kernel;
}

namespace {

string ComposeCreateTableColumnSpecs() {
  const ColumnSpec* begin = g_metas_columns;
  const ColumnSpec* end = g_metas_columns + arraysize(g_metas_columns);
  string query;
  query.reserve(kUpdateStatementBufferSize);
  char separator = '(';
  for (const ColumnSpec* column = begin; column != end; ++column) {
    query.push_back(separator);
    separator = ',';
    query.append(column->name);
    query.push_back(' ');
    query.append(column->spec);
  }
  query.push_back(')');
  return query;
}

void AppendColumnList(std::string* output) {
  const char* joiner = " ";
  // Be explicit in SELECT order to match up with UnpackEntry.
  for (int i = BEGIN_FIELDS; i < BEGIN_FIELDS + FIELD_COUNT; ++i) {
    output->append(joiner);
    output->append(ColumnName(i));
    joiner = ", ";
  }
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// DirectoryBackingStore implementation.

DirectoryBackingStore::DirectoryBackingStore(const string& dir_name)
  : db_(new sql::Connection()),
    dir_name_(dir_name),
    needs_column_refresh_(false) {
}

DirectoryBackingStore::DirectoryBackingStore(const string& dir_name,
                                             sql::Connection* db)
  : db_(db),
    dir_name_(dir_name),
    needs_column_refresh_(false) {
}

DirectoryBackingStore::~DirectoryBackingStore() {
}

bool DirectoryBackingStore::DeleteEntries(const MetahandleSet& handles) {
  if (handles.empty())
    return true;

  sql::Statement statement(db_->GetCachedStatement(
          SQL_FROM_HERE, "DELETE FROM metas WHERE metahandle = ?"));

  for (MetahandleSet::const_iterator i = handles.begin(); i != handles.end();
       ++i) {
    statement.BindInt64(0, *i);
    if (!statement.Run())
      return false;
    statement.Reset(true);
  }
  return true;
}

bool DirectoryBackingStore::SaveChanges(
    const Directory::SaveChangesSnapshot& snapshot) {
  DCHECK(CalledOnValidThread());
  DCHECK(db_->is_open());

  // Back out early if there is nothing to write.
  bool save_info =
    (Directory::KERNEL_SHARE_INFO_DIRTY == snapshot.kernel_info_status);
  if (snapshot.dirty_metas.size() < 1 && !save_info)
    return true;

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  for (EntryKernelSet::const_iterator i = snapshot.dirty_metas.begin();
       i != snapshot.dirty_metas.end(); ++i) {
    DCHECK(i->is_dirty());
    if (!SaveEntryToDB(*i))
      return false;
  }

  if (!DeleteEntries(snapshot.metahandles_to_purge))
    return false;

  if (save_info) {
    const Directory::PersistedKernelInfo& info = snapshot.kernel_info;
    sql::Statement s1(db_->GetCachedStatement(
            SQL_FROM_HERE,
            "UPDATE share_info "
            "SET store_birthday = ?, "
            "next_id = ?, "
            "notification_state = ?, "
            "bag_of_chips = ?"));
    s1.BindString(0, info.store_birthday);
    s1.BindInt64(1, info.next_id);
    s1.BindBlob(2, info.notification_state.data(),
                   info.notification_state.size());
    s1.BindBlob(3, info.bag_of_chips.data(), info.bag_of_chips.size());

    if (!s1.Run())
      return false;
    DCHECK_EQ(db_->GetLastChangeCount(), 1);

    sql::Statement s2(db_->GetCachedStatement(
            SQL_FROM_HERE,
            "INSERT OR REPLACE "
            "INTO models (model_id, progress_marker, initial_sync_ended) "
            "VALUES (?, ?, ?)"));

    for (int i = FIRST_REAL_MODEL_TYPE; i < MODEL_TYPE_COUNT; ++i) {
      // We persist not ModelType but rather a protobuf-derived ID.
      string model_id = ModelTypeEnumToModelId(ModelTypeFromInt(i));
      string progress_marker;
      info.download_progress[i].SerializeToString(&progress_marker);
      s2.BindBlob(0, model_id.data(), model_id.length());
      s2.BindBlob(1, progress_marker.data(), progress_marker.length());
      s2.BindBool(2, info.initial_sync_ended.Has(ModelTypeFromInt(i)));
      if (!s2.Run())
        return false;
      DCHECK_EQ(db_->GetLastChangeCount(), 1);
      s2.Reset(true);
    }
  }

  return transaction.Commit();
}

bool DirectoryBackingStore::InitializeTables() {
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  int version_on_disk = GetVersion();

  // Upgrade from version 67. Version 67 was widely distributed as the original
  // Bookmark Sync release. Version 68 removed unique naming.
  if (version_on_disk == 67) {
    if (MigrateVersion67To68())
      version_on_disk = 68;
  }
  // Version 69 introduced additional datatypes.
  if (version_on_disk == 68) {
    if (MigrateVersion68To69())
      version_on_disk = 69;
  }

  if (version_on_disk == 69) {
    if (MigrateVersion69To70())
      version_on_disk = 70;
  }

  // Version 71 changed the sync progress information to be per-datatype.
  if (version_on_disk == 70) {
    if (MigrateVersion70To71())
      version_on_disk = 71;
  }

  // Version 72 removed extended attributes, a legacy way to do extensible
  // key/value information, stored in their own table.
  if (version_on_disk == 71) {
    if (MigrateVersion71To72())
      version_on_disk = 72;
  }

  // Version 73 added a field for notification state.
  if (version_on_disk == 72) {
    if (MigrateVersion72To73())
      version_on_disk = 73;
  }

  // Version 74 added state for the autofill migration.
  if (version_on_disk == 73) {
    if (MigrateVersion73To74())
      version_on_disk = 74;
  }

  // Version 75 migrated from int64-based timestamps to per-datatype tokens.
  if (version_on_disk == 74) {
    if (MigrateVersion74To75())
      version_on_disk = 75;
  }

  // Version 76 removed all (5) autofill migration related columns.
  if (version_on_disk == 75) {
    if (MigrateVersion75To76())
      version_on_disk = 76;
  }

  // Version 77 standardized all time fields to ms since the Unix
  // epoch.
  if (version_on_disk == 76) {
    if (MigrateVersion76To77())
      version_on_disk = 77;
  }

  // Version 78 added the column base_server_specifics to the metas table.
  if (version_on_disk == 77) {
    if (MigrateVersion77To78())
      version_on_disk = 78;
  }

  // Version 79 migration is a one-time fix for some users in a bad state.
  if (version_on_disk == 78) {
    if (MigrateVersion78To79())
      version_on_disk = 79;
  }

  // Version 80 migration is adding the bag_of_chips column.
  if (version_on_disk == 79) {
    if (MigrateVersion79To80())
      version_on_disk = 80;
  }

  // If one of the migrations requested it, drop columns that aren't current.
  // It's only safe to do this after migrating all the way to the current
  // version.
  if (version_on_disk == kCurrentDBVersion && needs_column_refresh_) {
    if (!RefreshColumns())
      version_on_disk = 0;
  }

  // A final, alternative catch-all migration to simply re-sync everything.
  //
  // TODO(rlarocque): It's wrong to recreate the database here unless the higher
  // layers were expecting us to do so.  See crbug.com/103824.  We must leave
  // this code as is for now because this is the code that ends up creating the
  // database in the first time sync case, where the higher layers are expecting
  // us to create a fresh database.  The solution to this should be to implement
  // crbug.com/105018.
  if (version_on_disk != kCurrentDBVersion) {
    if (version_on_disk > kCurrentDBVersion)
      return false;

    // Fallback (re-sync everything) migration path.
    DVLOG(1) << "Old/null sync database, version " << version_on_disk;
    // Delete the existing database (if any), and create a fresh one.
    DropAllTables();
    if (!CreateTables())
      return false;
  }

  sql::Statement s(db_->GetUniqueStatement(
          "SELECT db_create_version, db_create_time FROM share_info"));
  if (!s.Step())
    return false;
  string db_create_version = s.ColumnString(0);
  int db_create_time = s.ColumnInt(1);
  DVLOG(1) << "DB created at " << db_create_time << " by version " <<
      db_create_version;

  return transaction.Commit();
}

// This function drops unused columns by creating a new table that contains only
// the currently used columns then copying all rows from the old tables into
// this new one.  The tables are then rearranged so the new replaces the old.
bool DirectoryBackingStore::RefreshColumns() {
  DCHECK(needs_column_refresh_);

  // Create a new table named temp_metas.
  SafeDropTable("temp_metas");
  if (!CreateMetasTable(true))
    return false;

  // Populate temp_metas from metas.
  //
  // At this point, the metas table may contain columns belonging to obsolete
  // schema versions.  This statement explicitly lists only the columns that
  // belong to the current schema version, so the obsolete columns will be
  // effectively dropped once we rename temp_metas over top of metas.
  std::string query = "INSERT INTO temp_metas (";
  AppendColumnList(&query);
  query.append(") SELECT ");
  AppendColumnList(&query);
  query.append(" FROM metas");
  if (!db_->Execute(query.c_str()))
    return false;

  // Drop metas.
  SafeDropTable("metas");

  // Rename temp_metas -> metas.
  if (!db_->Execute("ALTER TABLE temp_metas RENAME TO metas"))
    return false;

  // Repeat the process for share_info.
  SafeDropTable("temp_share_info");
  if (!CreateShareInfoTable(true))
    return false;

  if (!db_->Execute(
          "INSERT INTO temp_share_info (id, name, store_birthday, "
          "db_create_version, db_create_time, next_id, cache_guid,"
          "notification_state, bag_of_chips) "
          "SELECT id, name, store_birthday, db_create_version, "
          "db_create_time, next_id, cache_guid, notification_state, "
          "bag_of_chips "
          "FROM share_info"))
    return false;

  SafeDropTable("share_info");
  if (!db_->Execute("ALTER TABLE temp_share_info RENAME TO share_info"))
    return false;

  needs_column_refresh_ = false;
  return true;
}

bool DirectoryBackingStore::LoadEntries(MetahandlesIndex* entry_bucket) {
  string select;
  select.reserve(kUpdateStatementBufferSize);
  select.append("SELECT ");
  AppendColumnList(&select);
  select.append(" FROM metas ");

  sql::Statement s(db_->GetUniqueStatement(select.c_str()));

  while (s.Step()) {
    EntryKernel *kernel = UnpackEntry(&s);
    entry_bucket->insert(kernel);
  }
  return s.Succeeded();
}

bool DirectoryBackingStore::LoadInfo(Directory::KernelLoadInfo* info) {
  {
    sql::Statement s(
        db_->GetUniqueStatement(
            "SELECT store_birthday, next_id, cache_guid, notification_state, "
            "bag_of_chips "
            "FROM share_info"));
    if (!s.Step())
      return false;

    info->kernel_info.store_birthday = s.ColumnString(0);
    info->kernel_info.next_id = s.ColumnInt64(1);
    info->cache_guid = s.ColumnString(2);
    s.ColumnBlobAsString(3, &(info->kernel_info.notification_state));
    s.ColumnBlobAsString(4, &(info->kernel_info.bag_of_chips));

    // Verify there was only one row returned.
    DCHECK(!s.Step());
    DCHECK(s.Succeeded());
  }

  {
    sql::Statement s(
        db_->GetUniqueStatement(
            "SELECT model_id, progress_marker, initial_sync_ended "
            "FROM models"));

    while (s.Step()) {
      ModelType type = ModelIdToModelTypeEnum(s.ColumnBlob(0),
                                              s.ColumnByteLength(0));
      if (type != UNSPECIFIED && type != TOP_LEVEL_FOLDER) {
        info->kernel_info.download_progress[type].ParseFromArray(
            s.ColumnBlob(1), s.ColumnByteLength(1));
        if (s.ColumnBool(2))
          info->kernel_info.initial_sync_ended.Put(type);
      }
    }
    if (!s.Succeeded())
      return false;
  }
  {
    sql::Statement s(
        db_->GetUniqueStatement(
            "SELECT MAX(metahandle) FROM metas"));
    if (!s.Step())
      return false;

    info->max_metahandle = s.ColumnInt64(0);

    // Verify only one row was returned.
    DCHECK(!s.Step());
    DCHECK(s.Succeeded());
  }
  return true;
}

bool DirectoryBackingStore::SaveEntryToDB(const EntryKernel& entry) {
  // This statement is constructed at runtime, so we can't use
  // GetCachedStatement() to let the Connection cache it.   We will construct
  // and cache it ourselves the first time this function is called.
  if (!save_entry_statement_.is_valid()) {
    string query;
    query.reserve(kUpdateStatementBufferSize);
    query.append("INSERT OR REPLACE INTO metas ");
    string values;
    values.reserve(kUpdateStatementBufferSize);
    values.append("VALUES ");
    const char* separator = "( ";
    int i = 0;
    for (i = BEGIN_FIELDS; i < PROTO_FIELDS_END; ++i) {
      query.append(separator);
      values.append(separator);
      separator = ", ";
      query.append(ColumnName(i));
      values.append("?");
    }
    query.append(" ) ");
    values.append(" )");
    query.append(values);

    save_entry_statement_.Assign(
        db_->GetUniqueStatement(query.c_str()));
  } else {
    save_entry_statement_.Reset(true);
  }

  BindFields(entry, &save_entry_statement_);
  return save_entry_statement_.Run();
}

bool DirectoryBackingStore::DropDeletedEntries() {
  if (!db_->Execute("DELETE FROM metas "
                    "WHERE is_del > 0 "
                    "AND is_unsynced < 1 "
                    "AND is_unapplied_update < 1")) {
    return false;
  }
  if (!db_->Execute("DELETE FROM metas "
                    "WHERE is_del > 0 "
                    "AND id LIKE 'c%'")) {
    return false;
  }
  return true;
}

bool DirectoryBackingStore::SafeDropTable(const char* table_name) {
  string query = "DROP TABLE IF EXISTS ";
  query.append(table_name);
  return db_->Execute(query.c_str());
}

void DirectoryBackingStore::DropAllTables() {
  SafeDropTable("metas");
  SafeDropTable("temp_metas");
  SafeDropTable("share_info");
  SafeDropTable("temp_share_info");
  SafeDropTable("share_version");
  SafeDropTable("extended_attributes");
  SafeDropTable("models");
  SafeDropTable("temp_models");
  needs_column_refresh_ = false;
}

// static
ModelType DirectoryBackingStore::ModelIdToModelTypeEnum(
    const void* data, int size) {
  sync_pb::EntitySpecifics specifics;
  if (!specifics.ParseFromArray(data, size))
    return UNSPECIFIED;
  return GetModelTypeFromSpecifics(specifics);
}

// static
string DirectoryBackingStore::ModelTypeEnumToModelId(ModelType model_type) {
  sync_pb::EntitySpecifics specifics;
  AddDefaultFieldValue(model_type, &specifics);
  return specifics.SerializeAsString();
}

// static
std::string DirectoryBackingStore::GenerateCacheGUID() {
  // Generate a GUID with 128 bits of randomness.
  const int kGuidBytes = 128 / 8;
  std::string guid;
  base::Base64Encode(base::RandBytesAsString(kGuidBytes), &guid);
  return guid;
}

bool DirectoryBackingStore::MigrateToSpecifics(
    const char* old_columns,
    const char* specifics_column,
    void (*handler_function)(sql::Statement* old_value_query,
                             int old_value_column,
                             sync_pb::EntitySpecifics* mutable_new_value)) {
  std::string query_sql = base::StringPrintf(
      "SELECT metahandle, %s, %s FROM metas", specifics_column, old_columns);
  std::string update_sql = base::StringPrintf(
      "UPDATE metas SET %s = ? WHERE metahandle = ?", specifics_column);

  sql::Statement query(db_->GetUniqueStatement(query_sql.c_str()));
  sql::Statement update(db_->GetUniqueStatement(update_sql.c_str()));

  while (query.Step()) {
    int64 metahandle = query.ColumnInt64(0);
    std::string new_value_bytes;
    query.ColumnBlobAsString(1, &new_value_bytes);
    sync_pb::EntitySpecifics new_value;
    new_value.ParseFromString(new_value_bytes);
    handler_function(&query, 2, &new_value);
    new_value.SerializeToString(&new_value_bytes);

    update.BindBlob(0, new_value_bytes.data(), new_value_bytes.length());
    update.BindInt64(1, metahandle);
    if (!update.Run())
      return false;
    update.Reset(true);
  }
  return query.Succeeded();
}

bool DirectoryBackingStore::SetVersion(int version) {
  sql::Statement s(db_->GetCachedStatement(
          SQL_FROM_HERE, "UPDATE share_version SET data = ?"));
  s.BindInt(0, version);

  return s.Run();
}

int DirectoryBackingStore::GetVersion() {
  if (!db_->DoesTableExist("share_version"))
    return 0;

  sql::Statement statement(db_->GetUniqueStatement(
          "SELECT data FROM share_version"));
  if (statement.Step()) {
    return statement.ColumnInt(0);
  } else {
    return 0;
  }
}

bool DirectoryBackingStore::MigrateVersion67To68() {
  // This change simply removed three columns:
  //   string NAME
  //   string UNSANITIZED_NAME
  //   string SERVER_NAME
  // No data migration is necessary, but we should do a column refresh.
  SetVersion(68);
  needs_column_refresh_ = true;
  return true;
}

bool DirectoryBackingStore::MigrateVersion69To70() {
  // Added "unique_client_tag", renamed "singleton_tag" to unique_server_tag
  SetVersion(70);
  if (!db_->Execute(
          "ALTER TABLE metas ADD COLUMN unique_server_tag varchar"))
    return false;
  if (!db_->Execute(
          "ALTER TABLE metas ADD COLUMN unique_client_tag varchar"))
    return false;
  needs_column_refresh_ = true;

  if (!db_->Execute(
          "UPDATE metas SET unique_server_tag = singleton_tag"))
    return false;

  return true;
}

namespace {

// Callback passed to MigrateToSpecifics for the v68->v69 migration.  See
// MigrateVersion68To69().
void EncodeBookmarkURLAndFavicon(sql::Statement* old_value_query,
                                 int old_value_column,
                                 sync_pb::EntitySpecifics* mutable_new_value) {
  // Extract data from the column trio we expect.
  bool old_is_bookmark_object = old_value_query->ColumnBool(old_value_column);
  std::string old_url = old_value_query->ColumnString(old_value_column + 1);
  std::string old_favicon;
  old_value_query->ColumnBlobAsString(old_value_column + 2, &old_favicon);
  bool old_is_dir = old_value_query->ColumnBool(old_value_column + 3);

  if (old_is_bookmark_object) {
    sync_pb::BookmarkSpecifics* bookmark_data =
        mutable_new_value->mutable_bookmark();
    if (!old_is_dir) {
      bookmark_data->set_url(old_url);
      bookmark_data->set_favicon(old_favicon);
    }
  }
}

}  // namespace

bool DirectoryBackingStore::MigrateVersion68To69() {
  // In Version 68, there were columns on table 'metas':
  //   string BOOKMARK_URL
  //   string SERVER_BOOKMARK_URL
  //   blob BOOKMARK_FAVICON
  //   blob SERVER_BOOKMARK_FAVICON
  // In version 69, these columns went away in favor of storing
  // a serialized EntrySpecifics protobuf in the columns:
  //   protobuf blob SPECIFICS
  //   protobuf blob SERVER_SPECIFICS
  // For bookmarks, EntrySpecifics is extended as per
  // bookmark_specifics.proto. This migration converts bookmarks from the
  // former scheme to the latter scheme.

  // First, add the two new columns to the schema.
  if (!db_->Execute(
          "ALTER TABLE metas ADD COLUMN specifics blob"))
    return false;
  if (!db_->Execute(
          "ALTER TABLE metas ADD COLUMN server_specifics blob"))
    return false;

  // Next, fold data from the old columns into the new protobuf columns.
  if (!MigrateToSpecifics(("is_bookmark_object, bookmark_url, "
                           "bookmark_favicon, is_dir"),
                          "specifics",
                          &EncodeBookmarkURLAndFavicon)) {
    return false;
  }
  if (!MigrateToSpecifics(("server_is_bookmark_object, "
                           "server_bookmark_url, "
                           "server_bookmark_favicon, "
                           "server_is_dir"),
                          "server_specifics",
                          &EncodeBookmarkURLAndFavicon)) {
    return false;
  }

  // Lastly, fix up the "Google Chrome" folder, which is of the TOP_LEVEL_FOLDER
  // ModelType: it shouldn't have BookmarkSpecifics.
  if (!db_->Execute(
          "UPDATE metas SET specifics = NULL, server_specifics = NULL WHERE "
          "singleton_tag IN ('google_chrome')"))
    return false;

  SetVersion(69);
  needs_column_refresh_ = true;  // Trigger deletion of old columns.
  return true;
}

// Version 71, the columns 'initial_sync_ended' and 'last_sync_timestamp'
// were removed from the share_info table.  They were replaced by
// the 'models' table, which has these values on a per-datatype basis.
bool DirectoryBackingStore::MigrateVersion70To71() {
  if (!CreateV71ModelsTable())
    return false;

  // Move data from the old share_info columns to the new models table.
  {
    sql::Statement fetch(db_->GetUniqueStatement(
            "SELECT last_sync_timestamp, initial_sync_ended FROM share_info"));
    if (!fetch.Step())
      return false;

    int64 last_sync_timestamp = fetch.ColumnInt64(0);
    bool initial_sync_ended = fetch.ColumnBool(1);

    // Verify there were no additional rows returned.
    DCHECK(!fetch.Step());
    DCHECK(fetch.Succeeded());

    sql::Statement update(db_->GetUniqueStatement(
            "INSERT INTO models (model_id, "
            "last_download_timestamp, initial_sync_ended) VALUES (?, ?, ?)"));
    string bookmark_model_id = ModelTypeEnumToModelId(BOOKMARKS);
    update.BindBlob(0, bookmark_model_id.data(), bookmark_model_id.size());
    update.BindInt64(1, last_sync_timestamp);
    update.BindBool(2, initial_sync_ended);

    if (!update.Run())
      return false;
  }

  // Drop the columns from the old share_info table via a temp table.
  const bool kCreateAsTempShareInfo = true;

  if (!CreateShareInfoTableVersion71(kCreateAsTempShareInfo))
    return false;
  if (!db_->Execute(
          "INSERT INTO temp_share_info (id, name, store_birthday, "
          "db_create_version, db_create_time, next_id, cache_guid) "
          "SELECT id, name, store_birthday, db_create_version, "
          "db_create_time, next_id, cache_guid FROM share_info"))
    return false;
  SafeDropTable("share_info");
  if (!db_->Execute(
          "ALTER TABLE temp_share_info RENAME TO share_info"))
    return false;
  SetVersion(71);
  return true;
}

bool DirectoryBackingStore::MigrateVersion71To72() {
  // Version 72 removed a table 'extended_attributes', whose
  // contents didn't matter.
  SafeDropTable("extended_attributes");
  SetVersion(72);
  return true;
}

bool DirectoryBackingStore::MigrateVersion72To73() {
  // Version 73 added one column to the table 'share_info': notification_state
  if (!db_->Execute(
          "ALTER TABLE share_info ADD COLUMN notification_state BLOB"))
    return false;
  SetVersion(73);
  return true;
}

bool DirectoryBackingStore::MigrateVersion73To74() {
  // Version 74 added the following columns to the table 'share_info':
  //   autofill_migration_state
  //   bookmarks_added_during_autofill_migration
  //   autofill_migration_time
  //   autofill_entries_added_during_migration
  //   autofill_profiles_added_during_migration

  if (!db_->Execute(
          "ALTER TABLE share_info ADD COLUMN "
          "autofill_migration_state INT default 0"))
    return false;

  if (!db_->Execute(
          "ALTER TABLE share_info ADD COLUMN "
          "bookmarks_added_during_autofill_migration "
          "INT default 0"))
    return false;

  if (!db_->Execute(
          "ALTER TABLE share_info ADD COLUMN autofill_migration_time "
          "INT default 0"))
    return false;

  if (!db_->Execute(
          "ALTER TABLE share_info ADD COLUMN "
          "autofill_entries_added_during_migration "
          "INT default 0"))
    return false;

  if (!db_->Execute(
          "ALTER TABLE share_info ADD COLUMN "
          "autofill_profiles_added_during_migration "
          "INT default 0"))
    return false;

  SetVersion(74);
  return true;
}

bool DirectoryBackingStore::MigrateVersion74To75() {
  // In version 74, there was a table 'models':
  //     blob model_id (entity specifics, primary key)
  //     int last_download_timestamp
  //     boolean initial_sync_ended
  // In version 75, we deprecated the integer-valued last_download_timestamp,
  // using insted a protobuf-valued progress_marker field:
  //     blob progress_marker
  // The progress_marker values are initialized from the value of
  // last_download_timestamp, thereby preserving the download state.

  // Move aside the old table and create a new empty one at the current schema.
  if (!db_->Execute("ALTER TABLE models RENAME TO temp_models"))
    return false;
  if (!CreateModelsTable())
    return false;

  sql::Statement query(db_->GetUniqueStatement(
          "SELECT model_id, last_download_timestamp, initial_sync_ended "
          "FROM temp_models"));

  sql::Statement update(db_->GetUniqueStatement(
          "INSERT INTO models (model_id, "
          "progress_marker, initial_sync_ended) VALUES (?, ?, ?)"));

  while (query.Step()) {
    ModelType type = ModelIdToModelTypeEnum(query.ColumnBlob(0),
                                            query.ColumnByteLength(0));
    if (type != UNSPECIFIED) {
      // Set the |timestamp_token_for_migration| on a new
      // DataTypeProgressMarker, using the old value of last_download_timestamp.
      // The server will turn this into a real token on our behalf the next
      // time we check for updates.
      sync_pb::DataTypeProgressMarker progress_marker;
      progress_marker.set_data_type_id(
          GetSpecificsFieldNumberFromModelType(type));
      progress_marker.set_timestamp_token_for_migration(query.ColumnInt64(1));
      std::string progress_blob;
      progress_marker.SerializeToString(&progress_blob);

      update.BindBlob(0, query.ColumnBlob(0), query.ColumnByteLength(0));
      update.BindBlob(1, progress_blob.data(), progress_blob.length());
      update.BindBool(2, query.ColumnBool(2));
      if (!update.Run())
        return false;
      update.Reset(true);
    }
  }
  if (!query.Succeeded())
    return false;

  // Drop the old table.
  SafeDropTable("temp_models");

  SetVersion(75);
  return true;
}

bool DirectoryBackingStore::MigrateVersion75To76() {
  // This change removed five columns:
  //   autofill_migration_state
  //   bookmarks_added_during_autofill_migration
  //   autofill_migration_time
  //   autofill_entries_added_during_migration
  //   autofill_profiles_added_during_migration
  // No data migration is necessary, but we should do a column refresh.
  SetVersion(76);
  needs_column_refresh_ = true;
  return true;
}

bool DirectoryBackingStore::MigrateVersion76To77() {
  // This change changes the format of stored timestamps to ms since
  // the Unix epoch.
#if defined(OS_WIN)
// On Windows, we used to store timestamps in FILETIME format (100s of
// ns since Jan 1, 1601).  Magic numbers taken from
// http://stackoverflow.com/questions/5398557/java-library-for-dealing-with-win32-filetime
// .
#define TO_UNIX_TIME_MS(x) #x " = " #x " / 10000 - 11644473600000"
#else
// On other platforms, we used to store timestamps in time_t format (s
// since the Unix epoch).
#define TO_UNIX_TIME_MS(x) #x " = " #x " * 1000"
#endif
  sql::Statement update_timestamps(db_->GetUniqueStatement(
          "UPDATE metas SET "
          TO_UNIX_TIME_MS(mtime) ", "
          TO_UNIX_TIME_MS(server_mtime) ", "
          TO_UNIX_TIME_MS(ctime) ", "
          TO_UNIX_TIME_MS(server_ctime)));
#undef TO_UNIX_TIME_MS
  if (!update_timestamps.Run())
    return false;
  SetVersion(77);
  return true;
}

bool DirectoryBackingStore::MigrateVersion77To78() {
  // Version 78 added one column to table 'metas': base_server_specifics.
  if (!db_->Execute(
          "ALTER TABLE metas ADD COLUMN base_server_specifics BLOB")) {
    return false;
  }
  SetVersion(78);
  return true;
}

bool DirectoryBackingStore::MigrateVersion78To79() {
  // Some users are stuck with a DB that causes them to reuse existing IDs.  We
  // perform this one-time fixup on all users to help the few that are stuck.
  // See crbug.com/142987 for details.
  if (!db_->Execute(
          "UPDATE share_info SET next_id = next_id - 65536")) {
    return false;
  }
  SetVersion(79);
  return true;
}
bool DirectoryBackingStore::MigrateVersion79To80() {
  if (!db_->Execute(
          "ALTER TABLE share_info ADD COLUMN bag_of_chips BLOB"))
    return false;
  sql::Statement update(db_->GetUniqueStatement(
          "UPDATE share_info SET bag_of_chips = ?"));
  // An empty message is serialized to an empty string.
  update.BindBlob(0, NULL, 0);
  if (!update.Run())
    return false;
  SetVersion(80);
  return true;
}

bool DirectoryBackingStore::CreateTables() {
  DVLOG(1) << "First run, creating tables";
  // Create two little tables share_version and share_info
  if (!db_->Execute(
          "CREATE TABLE share_version ("
          "id VARCHAR(128) primary key, data INT)")) {
    return false;
  }

  {
    sql::Statement s(db_->GetUniqueStatement(
            "INSERT INTO share_version VALUES(?, ?)"));
    s.BindString(0, dir_name_);
    s.BindInt(1, kCurrentDBVersion);

    if (!s.Run())
      return false;
  }

  const bool kCreateAsTempShareInfo = false;
  if (!CreateShareInfoTable(kCreateAsTempShareInfo)) {
    return false;
  }

  {
    sql::Statement s(db_->GetUniqueStatement(
            "INSERT INTO share_info VALUES"
            "(?, "  // id
            "?, "   // name
            "?, "   // store_birthday
            "?, "   // db_create_version
            "?, "   // db_create_time
            "-2, "  // next_id
            "?, "   // cache_guid
            "?, "   // notification_state
            "?);"));  // bag_of_chips
    s.BindString(0, dir_name_);                   // id
    s.BindString(1, dir_name_);                   // name
    s.BindString(2, "");                          // store_birthday
    // TODO(akalin): Remove this unused db_create_version field. (Or
    // actually use it for something.) http://crbug.com/118356
    s.BindString(3, "Unknown");                   // db_create_version
    s.BindInt(4, static_cast<int32>(time(0)));    // db_create_time
    s.BindString(5, GenerateCacheGUID());         // cache_guid
    s.BindBlob(6, NULL, 0);                       // notification_state
    s.BindBlob(7, NULL, 0);                       // bag_of_chips
    if (!s.Run())
      return false;
  }

  if (!CreateModelsTable())
    return false;

  // Create the big metas table.
  if (!CreateMetasTable(false))
    return false;

  {
    // Insert the entry for the root into the metas table.
    const int64 now = TimeToProtoTime(base::Time::Now());
    sql::Statement s(db_->GetUniqueStatement(
            "INSERT INTO metas "
            "( id, metahandle, is_dir, ctime, mtime) "
            "VALUES ( \"r\", 1, 1, ?, ?)"));
    s.BindInt64(0, now);
    s.BindInt64(1, now);

    if (!s.Run())
      return false;
  }

  return true;
}

bool DirectoryBackingStore::CreateMetasTable(bool is_temporary) {
  const char* name = is_temporary ? "temp_metas" : "metas";
  string query = "CREATE TABLE ";
  query.append(name);
  query.append(ComposeCreateTableColumnSpecs());
  return db_->Execute(query.c_str());
}

bool DirectoryBackingStore::CreateV71ModelsTable() {
  // This is an old schema for the Models table, used from versions 71 to 74.
  return db_->Execute(
      "CREATE TABLE models ("
      "model_id BLOB primary key, "
      "last_download_timestamp INT, "
      // Gets set if the syncer ever gets updates from the
      // server and the server returns 0.  Lets us detect the
      // end of the initial sync.
      "initial_sync_ended BOOLEAN default 0)");
}

bool DirectoryBackingStore::CreateModelsTable() {
  // This is the current schema for the Models table, from version 75
  // onward.  If you change the schema, you'll probably want to double-check
  // the use of this function in the v74-v75 migration.
  return db_->Execute(
      "CREATE TABLE models ("
      "model_id BLOB primary key, "
      "progress_marker BLOB, "
      // Gets set if the syncer ever gets updates from the
      // server and the server returns 0.  Lets us detect the
      // end of the initial sync.
      "initial_sync_ended BOOLEAN default 0)");
}

bool DirectoryBackingStore::CreateShareInfoTable(bool is_temporary) {
  const char* name = is_temporary ? "temp_share_info" : "share_info";
  string query = "CREATE TABLE ";
  query.append(name);
  // This is the current schema for the ShareInfo table, from version 76
  // onward.
  query.append(" ("
      "id TEXT primary key, "
      "name TEXT, "
      "store_birthday TEXT, "
      "db_create_version TEXT, "
      "db_create_time INT, "
      "next_id INT default -2, "
      "cache_guid TEXT, "
      "notification_state BLOB, "
      "bag_of_chips BLOB"
      ")");
  return db_->Execute(query.c_str());
}

bool DirectoryBackingStore::CreateShareInfoTableVersion71(
    bool is_temporary) {
  const char* name = is_temporary ? "temp_share_info" : "share_info";
  string query = "CREATE TABLE ";
  query.append(name);
  // This is the schema for the ShareInfo table used from versions 71 to 72.
  query.append(" ("
      "id TEXT primary key, "
      "name TEXT, "
      "store_birthday TEXT, "
      "db_create_version TEXT, "
      "db_create_time INT, "
      "next_id INT default -2, "
      "cache_guid TEXT )");
  return db_->Execute(query.c_str());
}

// This function checks to see if the given list of Metahandles has any nodes
// whose PREV_ID, PARENT_ID or NEXT_ID values refer to ID values that do not
// actually exist.  Returns true on success.
bool DirectoryBackingStore::VerifyReferenceIntegrity(
    const syncable::MetahandlesIndex &index) {
  TRACE_EVENT0("sync", "SyncDatabaseIntegrityCheck");
  using namespace syncable;
  typedef base::hash_set<std::string> IdsSet;

  IdsSet ids_set;
  bool is_ok = true;

  for (MetahandlesIndex::const_iterator it = index.begin();
       it != index.end(); ++it) {
    EntryKernel* entry = *it;
    bool is_duplicate_id = !(ids_set.insert(entry->ref(ID).value()).second);
    is_ok = is_ok && !is_duplicate_id;
  }

  IdsSet::iterator end = ids_set.end();
  for (MetahandlesIndex::const_iterator it = index.begin();
       it != index.end(); ++it) {
    EntryKernel* entry = *it;
    bool prev_exists = (ids_set.find(entry->ref(PREV_ID).value()) != end);
    bool parent_exists = (ids_set.find(entry->ref(PARENT_ID).value()) != end);
    bool next_exists = (ids_set.find(entry->ref(NEXT_ID).value()) != end);
    is_ok = is_ok && prev_exists && parent_exists && next_exists;
  }
  return is_ok;
}

}  // namespace syncable
}  // namespace syncer
