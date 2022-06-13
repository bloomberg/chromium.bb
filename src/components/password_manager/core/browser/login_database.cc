// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/login_database.h"

#include <stddef.h>
#include <stdint.h>
#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/pickle.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/os_crypt/os_crypt.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/insecure_credentials_table.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "components/password_manager/core/browser/psl_matching_helper.h"
#include "components/password_manager/core/browser/sql_table_builder.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/origin.h"
#include "url/url_constants.h"

using autofill::GaiaIdHash;

namespace password_manager {

// The current version number of the login database schema.
constexpr int kCurrentVersionNumber = 31;
// The oldest version of the schema such that a legacy Chrome client using that
// version can still read/write the current database.
constexpr int kCompatibleVersionNumber = 31;

base::Pickle SerializeValueElementPairs(const ValueElementVector& vec) {
  base::Pickle p;
  for (const auto& pair : vec) {
    p.WriteString16(pair.first);
    p.WriteString16(pair.second);
  }
  return p;
}

ValueElementVector DeserializeValueElementPairs(const base::Pickle& p) {
  ValueElementVector ret;
  std::u16string value;
  std::u16string field_name;

  base::PickleIterator iterator(p);
  while (iterator.ReadString16(&value)) {
    bool name_success = iterator.ReadString16(&field_name);
    DCHECK(name_success);
    ret.push_back(ValueElementPair(value, field_name));
  }
  return ret;
}

base::Pickle SerializeGaiaIdHashVector(const std::vector<GaiaIdHash>& hashes) {
  base::Pickle p;
  for (const auto& hash : hashes)
    p.WriteString(hash.ToBinary());
  return p;
}

std::vector<GaiaIdHash> DeserializeGaiaIdHashVector(const base::Pickle& p) {
  std::vector<GaiaIdHash> hashes;
  std::string hash;

  base::PickleIterator iterator(p);
  while (iterator.ReadString(&hash))
    hashes.push_back(GaiaIdHash::FromBinary(hash));
  return hashes;
}

namespace {

// Common prefix for all histograms.
constexpr char kPasswordManager[] = "PasswordManager";

// A simple class for scoping a login database transaction. This does not
// support rollback since the login database doesn't either.
class ScopedTransaction {
 public:
  explicit ScopedTransaction(LoginDatabase* db) : db_(db) {
    db_->BeginTransaction();
  }

  ScopedTransaction(const ScopedTransaction&) = delete;
  ScopedTransaction& operator=(const ScopedTransaction&) = delete;

  ~ScopedTransaction() { db_->CommitTransaction(); }

 private:
  raw_ptr<LoginDatabase> db_;
};

// Convenience enum for interacting with SQL queries that use all the columns.
enum LoginDatabaseTableColumns {
  COLUMN_ORIGIN_URL = 0,
  COLUMN_ACTION_URL,
  COLUMN_USERNAME_ELEMENT,
  COLUMN_USERNAME_VALUE,
  COLUMN_PASSWORD_ELEMENT,
  COLUMN_PASSWORD_VALUE,
  COLUMN_SUBMIT_ELEMENT,
  COLUMN_SIGNON_REALM,
  COLUMN_DATE_CREATED,
  COLUMN_BLOCKLISTED_BY_USER,
  COLUMN_SCHEME,
  COLUMN_PASSWORD_TYPE,
  COLUMN_TIMES_USED,
  COLUMN_FORM_DATA,
  COLUMN_DISPLAY_NAME,
  COLUMN_ICON_URL,
  COLUMN_FEDERATION_URL,
  COLUMN_SKIP_ZERO_CLICK,
  COLUMN_GENERATION_UPLOAD_STATUS,
  COLUMN_POSSIBLE_USERNAME_PAIRS,
  COLUMN_ID,
  COLUMN_DATE_LAST_USED,
  COLUMN_MOVING_BLOCKED_FOR,
  COLUMN_DATE_PASSWORD_MODIFIED,
  COLUMN_NUM  // Keep this last.
};

enum class HistogramSize { SMALL, LARGE };

// An enum for UMA reporting. Add values to the end only.
enum DatabaseInitError {
  INIT_OK = 0,
  OPEN_FILE_ERROR = 1,
  START_TRANSACTION_ERROR = 2,
  META_TABLE_INIT_ERROR = 3,
  INCOMPATIBLE_VERSION = 4,
  INIT_LOGINS_ERROR = 5,
  INIT_STATS_ERROR = 6,
  MIGRATION_ERROR = 7,
  COMMIT_TRANSACTION_ERROR = 8,
  INIT_COMPROMISED_CREDENTIALS_ERROR = 9,
  INIT_FIELD_INFO_ERROR = 10,
  FOREIGN_KEY_ERROR = 11,

  DATABASE_INIT_ERROR_COUNT,
};

// Struct to hold table builder for "logins", "insecure_credentials",
// "sync_entities_metadata", and "sync_model_metadata" tables.
struct SQLTableBuilders {
  SQLTableBuilder* logins;
  SQLTableBuilder* insecure_credentials;
  SQLTableBuilder* sync_entities_metadata;
  SQLTableBuilder* sync_model_metadata;
};

base::span<const uint8_t> PickleToSpan(const base::Pickle& pickle) {
  return base::make_span(reinterpret_cast<const uint8_t*>(pickle.data()),
                         pickle.size());
}

base::Pickle PickleFromSpan(base::span<const uint8_t> data) {
  return base::Pickle(reinterpret_cast<const char*>(data.data()), data.size());
}

void BindAddStatement(const PasswordForm& form, sql::Statement* s) {
  s->BindString(COLUMN_ORIGIN_URL, form.url.spec());
  s->BindString(COLUMN_ACTION_URL, form.action.spec());
  s->BindString16(COLUMN_USERNAME_ELEMENT, form.username_element);
  s->BindString16(COLUMN_USERNAME_VALUE, form.username_value);
  s->BindString16(COLUMN_PASSWORD_ELEMENT, form.password_element);
  s->BindBlob(COLUMN_PASSWORD_VALUE, form.encrypted_password);
  s->BindString16(COLUMN_SUBMIT_ELEMENT, form.submit_element);
  s->BindString(COLUMN_SIGNON_REALM, form.signon_realm);
  s->BindInt64(COLUMN_DATE_CREATED, form.date_created.ToInternalValue());
  s->BindInt(COLUMN_BLOCKLISTED_BY_USER, form.blocked_by_user);
  s->BindInt(COLUMN_SCHEME, static_cast<int>(form.scheme));
  s->BindInt(COLUMN_PASSWORD_TYPE, static_cast<int>(form.type));
  s->BindInt(COLUMN_TIMES_USED, form.times_used);
  base::Pickle form_data_pickle;
  autofill::SerializeFormData(form.form_data, &form_data_pickle);
  s->BindBlob(COLUMN_FORM_DATA, PickleToSpan(form_data_pickle));
  s->BindString16(COLUMN_DISPLAY_NAME, form.display_name);
  s->BindString(COLUMN_ICON_URL, form.icon_url.spec());
  // An empty Origin serializes as "null" which would be strange to store here.
  s->BindString(COLUMN_FEDERATION_URL,
                form.federation_origin.opaque()
                    ? std::string()
                    : form.federation_origin.Serialize());
  s->BindInt(COLUMN_SKIP_ZERO_CLICK, form.skip_zero_click);
  s->BindInt(COLUMN_GENERATION_UPLOAD_STATUS,
             static_cast<int>(form.generation_upload_status));
  base::Pickle usernames_pickle =
      SerializeValueElementPairs(form.all_possible_usernames);
  s->BindBlob(COLUMN_POSSIBLE_USERNAME_PAIRS, PickleToSpan(usernames_pickle));
  s->BindInt64(COLUMN_DATE_LAST_USED,
               form.date_last_used.ToDeltaSinceWindowsEpoch().InMicroseconds());
  base::Pickle moving_blocked_for_pickle =
      SerializeGaiaIdHashVector(form.moving_blocked_for_list);
  s->BindBlob(COLUMN_MOVING_BLOCKED_FOR,
              PickleToSpan(moving_blocked_for_pickle));
  s->BindInt64(
      COLUMN_DATE_PASSWORD_MODIFIED,
      form.date_password_modified.ToDeltaSinceWindowsEpoch().InMicroseconds());
}

// Output parameter is the first one because of binding order.
void AddCallback(int* output_err, int err, sql::Statement* /*stmt*/) {
  DCHECK(output_err);
  *output_err = err;
  if (err == 19 /*SQLITE_CONSTRAINT*/)
    DLOG(WARNING) << "LoginDatabase::AddLogin updated an existing form";
}

bool DoesMatchConstraints(const PasswordForm& form) {
  if (!IsValidAndroidFacetURI(form.signon_realm) && form.url.is_empty()) {
    DLOG(ERROR) << "Constraint violation: form.origin is empty";
    return false;
  }
  if (form.signon_realm.empty()) {
    DLOG(ERROR) << "Constraint violation: form.signon_realm is empty";
    return false;
  }
  return true;
}

void LogDatabaseInitError(DatabaseInitError error) {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.LoginDatabaseInit", error,
                            DATABASE_INIT_ERROR_COUNT);
}

bool ClearAllSyncMetadata(sql::Database* db) {
  sql::Statement s1(
      db->GetCachedStatement(SQL_FROM_HERE, "DELETE FROM sync_model_metadata"));

  sql::Statement s2(db->GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM sync_entities_metadata"));

  return s1.Run() && s2.Run();
}

// Seals the version of the given builders. This is method should be always used
// to seal versions of all builder to make sure all builders are at the same
// version.
void SealVersion(SQLTableBuilders builders, unsigned expected_version) {
  unsigned logins_version = builders.logins->SealVersion();
  DCHECK_EQ(expected_version, logins_version);

  unsigned insecure_credentials_version =
      builders.insecure_credentials->SealVersion();
  DCHECK_EQ(expected_version, insecure_credentials_version);

  unsigned sync_entities_metadata_version =
      builders.sync_entities_metadata->SealVersion();
  DCHECK_EQ(expected_version, sync_entities_metadata_version);

  unsigned sync_model_metadata_version =
      builders.sync_model_metadata->SealVersion();
  DCHECK_EQ(expected_version, sync_model_metadata_version);
}

// Teaches |builders| about the different DB schemes in different versions.
void InitializeBuilders(SQLTableBuilders builders) {
  // Versions 0 and 1, which are the same.
  builders.logins->AddColumnToUniqueKey("origin_url", "VARCHAR NOT NULL");
  builders.logins->AddColumn("action_url", "VARCHAR");
  builders.logins->AddColumnToUniqueKey("username_element", "VARCHAR");
  builders.logins->AddColumnToUniqueKey("username_value", "VARCHAR");
  builders.logins->AddColumnToUniqueKey("password_element", "VARCHAR");
  builders.logins->AddColumn("password_value", "BLOB");
  builders.logins->AddColumn("submit_element", "VARCHAR");
  builders.logins->AddColumnToUniqueKey("signon_realm", "VARCHAR NOT NULL");
  builders.logins->AddColumn("ssl_valid", "INTEGER NOT NULL");
  builders.logins->AddColumn("preferred", "INTEGER NOT NULL");
  builders.logins->AddColumn("date_created", "INTEGER NOT NULL");
  builders.logins->AddColumn("blacklisted_by_user", "INTEGER NOT NULL");
  builders.logins->AddColumn("scheme", "INTEGER NOT NULL");
  builders.logins->AddIndex("logins_signon", {"signon_realm"});
  SealVersion(builders, /*expected_version=*/0u);
  SealVersion(builders, /*expected_version=*/1u);

  // Version 2.
  builders.logins->AddColumn("password_type", "INTEGER");
  builders.logins->AddColumn("possible_usernames", "BLOB");
  SealVersion(builders, /*expected_version=*/2u);

  // Version 3.
  builders.logins->AddColumn("times_used", "INTEGER");
  SealVersion(builders, /*expected_version=*/3u);

  // Version 4.
  builders.logins->AddColumn("form_data", "BLOB");
  SealVersion(builders, /*expected_version=*/4u);

  // Version 5.
  builders.logins->AddColumn("use_additional_auth", "INTEGER");
  SealVersion(builders, /*expected_version=*/5u);

  // Version 6.
  builders.logins->AddColumn("date_synced", "INTEGER");
  SealVersion(builders, /*expected_version=*/6u);

  // Version 7.
  builders.logins->AddColumn("display_name", "VARCHAR");
  builders.logins->AddColumn("avatar_url", "VARCHAR");
  builders.logins->AddColumn("federation_url", "VARCHAR");
  builders.logins->AddColumn("is_zero_click", "INTEGER");
  SealVersion(builders, /*expected_version=*/7u);

  // Version 8.
  SealVersion(builders, /*expected_version=*/8u);
  // Version 9.
  SealVersion(builders, /*expected_version=*/9u);
  // Version 10.
  builders.logins->DropColumn("use_additional_auth");
  SealVersion(builders, /*expected_version=*/10u);

  // Version 11.
  builders.logins->RenameColumn("is_zero_click", "skip_zero_click");
  SealVersion(builders, /*expected_version=*/11u);

  // Version 12.
  builders.logins->AddColumn("generation_upload_status", "INTEGER");
  SealVersion(builders, /*expected_version=*/12u);

  // Version 13.
  SealVersion(builders, /*expected_version=*/13u);
  // Version 14.
  builders.logins->RenameColumn("avatar_url", "icon_url");
  SealVersion(builders, /*expected_version=*/14u);

  // Version 15.
  SealVersion(builders, /*expected_version=*/15u);
  // Version 16.
  SealVersion(builders, /*expected_version=*/16u);
  // Version 17.
  SealVersion(builders, /*expected_version=*/17u);

  // Version 18.
  builders.logins->DropColumn("ssl_valid");
  SealVersion(builders, /*expected_version=*/18u);

  // Version 19.
  builders.logins->DropColumn("possible_usernames");
  builders.logins->AddColumn("possible_username_pairs", "BLOB");
  SealVersion(builders, /*expected_version=*/19u);

  // Version 20.
  builders.logins->AddPrimaryKeyColumn("id");
  SealVersion(builders, /*expected_version=*/20u);

  // Version 21.
  builders.sync_entities_metadata->AddPrimaryKeyColumn("storage_key");
  builders.sync_entities_metadata->AddColumn("metadata", "VARCHAR NOT NULL");
  builders.sync_model_metadata->AddPrimaryKeyColumn("id");
  builders.sync_model_metadata->AddColumn("model_metadata", "VARCHAR NOT NULL");
  SealVersion(builders, /*expected_version=*/21u);

  // Version 22. Changes in Sync metadata encryption.
  SealVersion(builders, /*expected_version=*/22u);

  // Version 23. Version 22 could have some corruption in Sync metadata and
  // hence we are migrating users on it by clearing their metadata to make Sync
  // start clean from scratch.
  SealVersion(builders, /*expected_version=*/23u);

  // Version 24. Version 23 could have some corruption in Sync metadata and
  // hence we are migrating users on it by clearing their metadata to make Sync
  // start clean from scratch.
  SealVersion(builders, /*expected_version=*/24u);

  // Version 25. Introduce date_last_used column to replace the preferred
  // column. MigrateDatabase() will take care of migrating the data.
  builders.logins->AddColumn("date_last_used", "INTEGER NOT NULL DEFAULT 0");
  SealVersion(builders, /*expected_version=*/25u);

  // Version 26 is the first version where the id is AUTOINCREMENT.
  SealVersion(builders, /*expected_version=*/26u);

  // Version 27. Add the moving_blocked_for column to contain serialized list of
  // gaia id hashes for users that prefer not to move this credential to their
  // account store.
  builders.logins->AddColumn("moving_blocked_for", "BLOB");
  SealVersion(builders, /*expected_version=*/27u);

  // Version 28.
  builders.logins->DropColumn("preferred");
  SealVersion(builders, /*expected_version=*/28u);

  // Version 29.
  // Migrate the compromised credentials from "compromised_credentials" to the
  // new table "insecure credentials" with a foreign key to the logins table.
  builders.insecure_credentials->AddColumnToUniqueKey("parent_id", "INTEGER",
                                                      "logins");
  builders.insecure_credentials->AddColumnToUniqueKey("insecurity_type",
                                                      "INTEGER NOT NULL");
  builders.insecure_credentials->AddColumn("create_time", "INTEGER NOT NULL");
  builders.insecure_credentials->AddColumn("is_muted",
                                           "INTEGER NOT NULL DEFAULT 0");
  SealVersion(builders, /*expected_version=*/29u);

  // Version 30. Introduce 'date_password_modified' column.
  builders.logins->AddColumn("date_password_modified",
                             "INTEGER NOT NULL DEFAULT 0");
  SealVersion(builders, /*expected_version=*/30u);

  // Version 31. Dropped 'date_synced' column.
  builders.logins->DropColumn("date_synced");
  SealVersion(builders, /*expected_version=*/31u);

  DCHECK_EQ(static_cast<size_t>(COLUMN_NUM), builders.logins->NumberOfColumns())
      << "Adjust LoginDatabaseTableColumns if you change column definitions "
         "here.";
}

// Callback called upon each migration step of the logins table. It's used to
// inject custom schema migration logic not covered by the generic
// SQLTableBuilder migration. |new_version| indicates how far
// SQLTableBuilder is in the migration process.
bool LoginsTablePostMigrationStepCallback(sql::Database* db,
                                          unsigned new_version) {
  // In version 26, the primary key of the logins table became an
  // AUTOINCREMENT field. Since SQLite doesn't allow changing the column type,
  // the only way is to actually create a temp table with the primary key
  // properly set as an AUTOINCREMENT field, and move the data there. The code
  // has been adjusted such that newly created tables have the primary key
  // properly set as AUTOINCREMENT.
  if (new_version == 26) {
    // This statement creates the logins database similar to version 26 with
    // the primary key column set to AUTOINCREMENT.
    const char temp_table_create_statement_version_26[] =
        "CREATE TABLE logins_temp (origin_url VARCHAR NOT NULL,action_url "
        "VARCHAR,username_element VARCHAR,username_value "
        "VARCHAR,password_element VARCHAR,password_value BLOB,submit_element "
        "VARCHAR,signon_realm VARCHAR NOT NULL,preferred INTEGER NOT "
        "NULL,date_created INTEGER NOT NULL,blacklisted_by_user INTEGER NOT "
        "NULL,scheme INTEGER NOT NULL,password_type INTEGER,times_used "
        "INTEGER,form_data BLOB,date_synced INTEGER,display_name "
        "VARCHAR,icon_url VARCHAR,federation_url VARCHAR,skip_zero_click "
        "INTEGER,generation_upload_status INTEGER,possible_username_pairs "
        "BLOB,id INTEGER PRIMARY KEY AUTOINCREMENT,date_last_used "
        "INTEGER,UNIQUE (origin_url, username_element, username_value, "
        "password_element, signon_realm))";
    const char move_data_statement[] =
        "INSERT INTO logins_temp SELECT * from logins";
    const char drop_table_statement[] = "DROP TABLE logins";
    const char rename_table_statement[] =
        "ALTER TABLE logins_temp RENAME TO logins";

    sql::Transaction transaction(db);
    if (!(transaction.Begin() &&
          db->Execute(temp_table_create_statement_version_26) &&
          db->Execute(move_data_statement) &&
          db->Execute(drop_table_statement) &&
          db->Execute(rename_table_statement) && transaction.Commit())) {
      return false;
    }
  }
  return true;
}

bool InsecureCredentialsPostMigrationStepCallback(
    SQLTableBuilder* insecure_credentials_builder,
    sql::Database* db,
    unsigned new_version) {
  if (new_version == 29) {
    if (!insecure_credentials_builder->CreateTable(db)) {
      LOG(ERROR) << "Failed to create the 'insecure_credentials' table";
      LogDatabaseInitError(INIT_COMPROMISED_CREDENTIALS_ERROR);
      return false;
    }
    if (!db->DoesTableExist("compromised_credentials"))
      return true;
    // The 'compromised_credentials' table must be migrated to
    // 'insecure_credentials'.
    constexpr char select_compromised[] =
        "SELECT "
        "id, create_time, compromise_type FROM compromised_credentials "
        "INNER JOIN logins ON "
        "compromised_credentials.url = logins.signon_realm AND "
        "compromised_credentials.username = logins.username_value";
    const std::string insert_statement = base::StringPrintf(
        "INSERT OR REPLACE INTO %s "
        "(parent_id, create_time, insecurity_type) %s",
        InsecureCredentialsTable::kTableName, select_compromised);
    constexpr char drop_table_statement[] =
        "DROP TABLE compromised_credentials";
    sql::Transaction transaction(db);
    if (!(transaction.Begin() && db->Execute(insert_statement.c_str()) &&
          db->Execute(drop_table_statement) && transaction.Commit())) {
      return false;
    }
  }
  return true;
}

// Call this after having called InitializeBuilders(), to migrate the database
// from the current version to kCurrentVersionNumber.
bool MigrateDatabase(unsigned current_version,
                     SQLTableBuilders builders,
                     sql::Database* db) {
  if (!builders.logins->MigrateFrom(
          current_version, db,
          base::BindRepeating(&LoginsTablePostMigrationStepCallback)))
    return false;
  if (!builders.insecure_credentials->MigrateFrom(
          current_version, db,
          base::BindRepeating(&InsecureCredentialsPostMigrationStepCallback,
                              builders.insecure_credentials)))
    return false;

  if (!builders.sync_entities_metadata->MigrateFrom(current_version, db))
    return false;

  if (!builders.sync_model_metadata->MigrateFrom(current_version, db))
    return false;

  // Data changes, not covered by the schema migration above.
  if (current_version <= 8) {
    sql::Statement fix_time_format;
    fix_time_format.Assign(db->GetUniqueStatement(
        "UPDATE logins SET date_created = (date_created * ?) + ?"));
    fix_time_format.BindInt64(0, base::Time::kMicrosecondsPerSecond);
    fix_time_format.BindInt64(1, base::Time::kTimeTToMicrosecondsOffset);
    if (!fix_time_format.Run())
      return false;
  }

  if (current_version <= 16) {
    sql::Statement reset_zero_click;
    reset_zero_click.Assign(
        db->GetUniqueStatement("UPDATE logins SET skip_zero_click = 1"));
    if (!reset_zero_click.Run())
      return false;
  }

  // Sync Metadata tables have been introduced in version 21. It is enough to
  // drop all data because Sync would populate the tables properly at startup.
  if (current_version >= 21 && current_version < 26) {
    if (!ClearAllSyncMetadata(db))
      return false;
  }

  // Set the default value for 'date_password_modified'.
  if (current_version < 30) {
    sql::Statement set_date_password_modified;
    set_date_password_modified.Assign(db->GetUniqueStatement(
        "UPDATE logins SET date_password_modified = date_created"));
    if (!set_date_password_modified.Run())
      return false;
  }

  return true;
}

// Because of https://crbug.com/295851, some early version numbers might be
// wrong. This function detects that and fixes the version.
bool FixVersionIfNeeded(sql::Database* db, int* current_version) {
  if (*current_version == 1) {
    int extra_columns = 0;
    if (db->DoesColumnExist("logins", "password_type"))
      ++extra_columns;
    if (db->DoesColumnExist("logins", "possible_usernames"))
      ++extra_columns;
    if (extra_columns == 2) {
      *current_version = 2;
    } else if (extra_columns == 1) {
      // If this is https://crbug.com/295851 then either both columns exist
      // or none.
      return false;
    }
  }
  if (*current_version == 2) {
    if (db->DoesColumnExist("logins", "times_used"))
      *current_version = 3;
  }
  if (*current_version == 3) {
    if (db->DoesColumnExist("logins", "form_data"))
      *current_version = 4;
  }
  // "date_last_used" columns has been introduced in version 25. if it exists,
  // the version should be at least 25. This has been added to address this bug
  // (crbug.com/1020320).
  if (*current_version < 25) {
    if (db->DoesColumnExist("logins", "date_last_used"))
      *current_version = 25;
  }
  return true;
}

// Generates the string "(?,?,...,?)" with |count| repetitions of "?".
std::string GeneratePlaceholders(size_t count) {
  std::string result(2 * count + 1, ',');
  result.front() = '(';
  result.back() = ')';
  for (size_t i = 1; i < 2 * count + 1; i += 2) {
    result[i] = '?';
  }
  return result;
}

#if defined(OS_MAC)
// Fills |form| with necessary data required to be removed from the database
// and returns it.
PasswordForm GetFormForRemoval(sql::Statement& statement) {
  PasswordForm form;
  form.url = GURL(statement.ColumnString(COLUMN_ORIGIN_URL));
  form.username_element = statement.ColumnString16(COLUMN_USERNAME_ELEMENT);
  form.username_value = statement.ColumnString16(COLUMN_USERNAME_VALUE);
  form.password_element = statement.ColumnString16(COLUMN_PASSWORD_ELEMENT);
  form.signon_realm = statement.ColumnString(COLUMN_SIGNON_REALM);
  return form;
}
#endif

}  // namespace

struct LoginDatabase::PrimaryKeyAndPassword {
  int primary_key;
  std::string encrypted_password;
  std::u16string decrypted_password;
};

LoginDatabase::LoginDatabase(const base::FilePath& db_path,
                             IsAccountStore is_account_store)
    : db_path_(db_path),
      is_account_store_(is_account_store),
      // Set options for a small, private database (based on WebDatabase).
      db_({.exclusive_locking = true, .page_size = 2048, .cache_size = 32}) {}

LoginDatabase::~LoginDatabase() = default;

bool LoginDatabase::Init() {
  TRACE_EVENT0("passwords", "LoginDatabase::Init");
  db_.set_histogram_tag("Passwords");

  if (!db_.Open(db_path_)) {
    LogDatabaseInitError(OPEN_FILE_ERROR);
    LOG(ERROR) << "Unable to open the password store database.";
    return false;
  }

  if (!db_.Execute("PRAGMA foreign_keys = ON")) {
    LogDatabaseInitError(FOREIGN_KEY_ERROR);
    LOG(ERROR) << "Unable to activate foreign keys.";
    return false;
  }

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    LogDatabaseInitError(START_TRANSACTION_ERROR);
    LOG(ERROR) << "Unable to start a transaction.";
    db_.Close();
    return false;
  }

  // Check the database version.
  if (!meta_table_.Init(&db_, kCurrentVersionNumber,
                        kCompatibleVersionNumber)) {
    LogDatabaseInitError(META_TABLE_INIT_ERROR);
    LOG(ERROR) << "Unable to create the meta table.";
    transaction.Rollback();
    db_.Close();
    return false;
  }
  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    LogDatabaseInitError(INCOMPATIBLE_VERSION);
    LOG(ERROR) << "Password store database is too new, kCurrentVersionNumber="
               << kCurrentVersionNumber << ", GetCompatibleVersionNumber="
               << meta_table_.GetCompatibleVersionNumber();
    transaction.Rollback();
    db_.Close();
    return false;
  }

  SQLTableBuilder logins_builder("logins");
  SQLTableBuilder insecure_credentials_builder(
      InsecureCredentialsTable::kTableName);
  SQLTableBuilder sync_entities_metadata_builder("sync_entities_metadata");
  SQLTableBuilder sync_model_metadata_builder("sync_model_metadata");
  SQLTableBuilders builders = {&logins_builder, &insecure_credentials_builder,
                               &sync_entities_metadata_builder,
                               &sync_model_metadata_builder};
  InitializeBuilders(builders);
  InitializeStatementStrings(logins_builder);

  if (!logins_builder.CreateTable(&db_)) {
    LOG(ERROR) << "Failed to create the 'logins' table";
    transaction.Rollback();
    db_.Close();
    return false;
  }

  if (!sync_entities_metadata_builder.CreateTable(&db_)) {
    LOG(ERROR) << "Failed to create the 'sync_entities_metadata' table";
    transaction.Rollback();
    db_.Close();
    return false;
  }

  if (!sync_model_metadata_builder.CreateTable(&db_)) {
    LOG(ERROR) << "Failed to create the 'sync_model_metadata' table";
    transaction.Rollback();
    db_.Close();
    return false;
  }

  stats_table_.Init(&db_);
  insecure_credentials_table_.Init(&db_);
  field_info_table_.Init(&db_);

  int current_version = meta_table_.GetVersionNumber();
  bool migration_success = FixVersionIfNeeded(&db_, &current_version);

  // If the file on disk is an older database version, bring it up to date.
  if (migration_success && current_version < kCurrentVersionNumber) {
    migration_success = MigrateDatabase(
        base::checked_cast<unsigned>(current_version), builders, &db_);
  }
  // Enforce that 'insecure_credentials' is created only after the 'logins'
  // table was created and migrated to the latest version. This guarantees the
  // existence of the `id` column in the `logins` table which was introduced
  // only in version 20 and is referenced by `insecure_credentials` table. The
  // table will be created here for a new profile. For an old profile it's
  // created in MigrateDatabase above.
  if (migration_success && !insecure_credentials_builder.CreateTable(&db_)) {
    LOG(ERROR) << "Failed to create the 'insecure_credentials' table";
    LogDatabaseInitError(INIT_COMPROMISED_CREDENTIALS_ERROR);
    transaction.Rollback();
    db_.Close();
    return false;
  }
  if (migration_success && current_version <= 15) {
    migration_success = stats_table_.MigrateToVersion(16);
  }
  if (migration_success) {
    // |migration_success| could be true when no logins have been actually
    // migrated. We should protect against downgrading the database version in
    // such case. Update the database version only if a migration took place.
    if (current_version < kCurrentVersionNumber) {
      meta_table_.SetCompatibleVersionNumber(kCompatibleVersionNumber);
      meta_table_.SetVersionNumber(kCurrentVersionNumber);
    }
  } else {
    LogDatabaseInitError(MIGRATION_ERROR);
    LOG(ERROR) << "Unable to migrate database from "
               << meta_table_.GetVersionNumber() << " to "
               << kCurrentVersionNumber;
    transaction.Rollback();
    db_.Close();
    return false;
  }

  if (!stats_table_.CreateTableIfNecessary()) {
    LogDatabaseInitError(INIT_STATS_ERROR);
    LOG(ERROR) << "Unable to create the stats table.";
    transaction.Rollback();
    db_.Close();
    return false;
  }

  // The table "leaked_credentials" was previously created without a flag.
  // The table is now renamed to "compromised_credentials" and also includes
  // a new column so the old table needs to be deleted.
  if (db_.DoesTableExist("leaked_credentials")) {
    if (!db_.Execute("DROP TABLE leaked_credentials")) {
      LOG(ERROR) << "Unable to create the stats table.";
      transaction.Rollback();
      db_.Close();
      return false;
    }
  }

  if (!field_info_table_.CreateTableIfNecessary()) {
    LogDatabaseInitError(INIT_FIELD_INFO_ERROR);
    LOG(ERROR) << "Unable to create the field info table.";
    transaction.Rollback();
    db_.Close();
    return false;
  }

  if (!transaction.Commit()) {
    LogDatabaseInitError(COMMIT_TRANSACTION_ERROR);
    LOG(ERROR) << "Unable to commit a transaction.";
    db_.Close();
    return false;
  }

  LogDatabaseInitError(INIT_OK);
  return true;
}

void LoginDatabase::ReportBubbleSuppressionMetrics() {
#if !defined(OS_IOS) && !defined(OS_ANDROID)
  base::UmaHistogramCustomCounts(
      "PasswordManager.BubbleSuppression.AccountsInStatisticsTable",
      stats_table_.GetNumAccounts(), 0, 1000, 100);
#endif  // !defined(OS_IOS) && !defined(OS_ANDROID)
}

void LoginDatabase::ReportInaccessiblePasswordsMetrics() {
  sql::Statement get_passwords_statement(
      db_.GetUniqueStatement("SELECT password_value "
                             "FROM logins WHERE blacklisted_by_user = 0"));

  size_t failed_encryption = 0;
  while (get_passwords_statement.Step()) {
    std::u16string decrypted_password;
    if (DecryptedString(get_passwords_statement.ColumnString(0),
                        &decrypted_password) != ENCRYPTION_RESULT_SUCCESS) {
      ++failed_encryption;
    }
  }
  // Note: For historic reasons, the profile store does not use a suffix, only
  // the account store does.
  base::StringPiece suffix_for_store =
      is_account_store_.value() ? ".AccountStore" : "";
  base::UmaHistogramCounts100(base::StrCat({kPasswordManager, suffix_for_store,
                                            ".InaccessiblePasswords"}),
                              failed_encryption);
}

void LoginDatabase::ReportMetrics() {
  TRACE_EVENT0("passwords", "LoginDatabase::ReportMetrics");

  ReportInaccessiblePasswordsMetrics();

  // BubbleSuppression fields aren't used in the account store.
  if (is_account_store_.value())
    return;
  ReportBubbleSuppressionMetrics();
}

PasswordStoreChangeList LoginDatabase::AddLogin(const PasswordForm& form,
                                                AddLoginError* error) {
  TRACE_EVENT0("passwords", "LoginDatabase::AddLogin");
  if (error) {
    *error = AddLoginError::kNone;
  }
  PasswordStoreChangeList list;
  if (!DoesMatchConstraints(form)) {
    if (error) {
      *error = AddLoginError::kConstraintViolation;
    }
    return list;
  }
  // [iOS] Passwords created in Credential Provider Extension (CPE) are already
  // encrypted in the keychain and there is no need to do the process again.
  // However, the password needs to be decryped instead so the actual password
  // syncs correctly.
  bool has_encrypted_password =
      !form.encrypted_password.empty() && form.password_value.empty();
  PasswordForm form_with_encrypted_password = form;
  if (has_encrypted_password) {
    std::u16string decrypted_password;
    if (DecryptedString(form.encrypted_password, &decrypted_password) !=
        ENCRYPTION_RESULT_SUCCESS) {
      if (error) {
        *error = AddLoginError::kEncrytionServiceFailure;
      }
      return list;
    }
    form_with_encrypted_password.password_value = decrypted_password;
  } else {
    std::string encrypted_password;
    if (EncryptedString(form.password_value, &encrypted_password) !=
        ENCRYPTION_RESULT_SUCCESS) {
      if (error) {
        *error = AddLoginError::kEncrytionServiceFailure;
      }
      return list;
    }
    form_with_encrypted_password.encrypted_password = encrypted_password;
  }

  DCHECK(!add_statement_.empty());
  sql::Statement s(
      db_.GetCachedStatement(SQL_FROM_HERE, add_statement_.c_str()));
  BindAddStatement(form_with_encrypted_password, &s);
  int sqlite_error_code;
  db_.set_error_callback(base::BindRepeating(&AddCallback, &sqlite_error_code));
  const bool success = s.Run();
  if (success) {
    // If success, the row never existed so password was not changed.
    FillFormInStore(&form_with_encrypted_password);
    FormPrimaryKey primary_key = FormPrimaryKey(db_.GetLastInsertRowId());
    if (!form_with_encrypted_password.password_issues.empty()) {
      UpdateInsecureCredentials(primary_key,
                                form_with_encrypted_password.password_issues);
    }
    list.emplace_back(PasswordStoreChange::ADD,
                      std::move(form_with_encrypted_password), primary_key,
                      /*password_changed=*/false);
    return list;
  }

  // Repeat the same statement but with REPLACE semantic.
  sqlite_error_code = 0;
  DCHECK(!add_replace_statement_.empty());
  PrimaryKeyAndPassword old_primary_key_password =
      GetPrimaryKeyAndPassword(form);
  bool password_changed =
      form.password_value != old_primary_key_password.decrypted_password;
  s.Assign(
      db_.GetCachedStatement(SQL_FROM_HERE, add_replace_statement_.c_str()));
  BindAddStatement(form_with_encrypted_password, &s);
  if (s.Run()) {
    PasswordForm removed_form = form;
    FillFormInStore(&removed_form);
    list.emplace_back(PasswordStoreChange::REMOVE, removed_form,
                      FormPrimaryKey(old_primary_key_password.primary_key));
    FillFormInStore(&form_with_encrypted_password);

    FormPrimaryKey primary_key = FormPrimaryKey(db_.GetLastInsertRowId());
    InsecureCredentialsChanged insecure_changed(false);
    if (!form_with_encrypted_password.password_issues.empty()) {
      insecure_changed = UpdateInsecureCredentials(
          primary_key, form_with_encrypted_password.password_issues);
    }
    list.emplace_back(PasswordStoreChange::ADD,
                      std::move(form_with_encrypted_password),
                      FormPrimaryKey(db_.GetLastInsertRowId()),
                      password_changed, insecure_changed);
  } else if (error) {
    if (sqlite_error_code == 19 /*SQLITE_CONSTRAINT*/) {
      *error = AddLoginError::kConstraintViolation;
    } else {
      *error = AddLoginError::kDbError;
    }
  }
  db_.reset_error_callback();
  return list;
}

PasswordStoreChangeList LoginDatabase::UpdateLogin(const PasswordForm& form,
                                                   UpdateLoginError* error) {
  TRACE_EVENT0("passwords", "LoginDatabase::UpdateLogin");
  if (error) {
    *error = UpdateLoginError::kNone;
  }
  std::string encrypted_password;
  if (EncryptedString(form.password_value, &encrypted_password) !=
      ENCRYPTION_RESULT_SUCCESS) {
    if (error) {
      *error = UpdateLoginError::kEncrytionServiceFailure;
    }
    return PasswordStoreChangeList();
  }

  const PrimaryKeyAndPassword old_primary_key_password =
      GetPrimaryKeyAndPassword(form);

#if defined(OS_IOS)
  DeleteEncryptedPasswordFromKeychain(
      old_primary_key_password.encrypted_password);
#endif
  DCHECK(!update_statement_.empty());
  sql::Statement s(
      db_.GetCachedStatement(SQL_FROM_HERE, update_statement_.c_str()));
  int next_param = 0;
  s.BindString(next_param++, form.action.spec());
  s.BindBlob(next_param++, encrypted_password);
  s.BindString16(next_param++, form.submit_element);
  s.BindInt64(next_param++, form.date_created.ToInternalValue());
  s.BindInt(next_param++, form.blocked_by_user);
  s.BindInt(next_param++, static_cast<int>(form.scheme));
  s.BindInt(next_param++, static_cast<int>(form.type));
  s.BindInt(next_param++, form.times_used);
  base::Pickle form_data_pickle;
  autofill::SerializeFormData(form.form_data, &form_data_pickle);
  s.BindBlob(next_param++, PickleToSpan(form_data_pickle));
  s.BindString16(next_param++, form.display_name);
  s.BindString(next_param++, form.icon_url.spec());
  // An empty Origin serializes as "null" which would be strange to store here.
  s.BindString(next_param++, form.federation_origin.opaque()
                                 ? std::string()
                                 : form.federation_origin.Serialize());
  s.BindInt(next_param++, form.skip_zero_click);
  s.BindInt(next_param++, static_cast<int>(form.generation_upload_status));
  base::Pickle username_pickle =
      SerializeValueElementPairs(form.all_possible_usernames);
  s.BindBlob(next_param++, PickleToSpan(username_pickle));
  s.BindInt64(next_param++,
              form.date_last_used.ToDeltaSinceWindowsEpoch().InMicroseconds());
  base::Pickle moving_blocked_for_pickle =
      SerializeGaiaIdHashVector(form.moving_blocked_for_list);
  s.BindBlob(next_param++, PickleToSpan(moving_blocked_for_pickle));
  s.BindInt64(
      next_param++,
      form.date_password_modified.ToDeltaSinceWindowsEpoch().InMicroseconds());
  // NOTE: Add new fields here unless the field is a part of the unique key.
  // If so, add new field below.

  // WHERE starts here.
  s.BindString(next_param++, form.url.spec());
  s.BindString16(next_param++, form.username_element);
  s.BindString16(next_param++, form.username_value);
  s.BindString16(next_param++, form.password_element);
  s.BindString(next_param++, form.signon_realm);
  // NOTE: Add new fields here only if the field is a part of the unique key.
  // Otherwise, add the field above "WHERE starts here" comment.

  if (!s.Run()) {
    if (error) {
      *error = UpdateLoginError::kDbError;
    }
    return PasswordStoreChangeList();
  }

  // If no rows changed due to this command, it means that there was no row to
  // update, so there is no point trying to update insecure credentials data.
  if (db_.GetLastChangeCount() == 0) {
    if (error) {
      *error = UpdateLoginError::kNoUpdatedRecords;
    }
    return PasswordStoreChangeList();
  }

  bool password_changed =
      form.password_value != old_primary_key_password.decrypted_password;

  PasswordForm form_with_encrypted_password = form;
  form_with_encrypted_password.encrypted_password = encrypted_password;

  // TODO(crbug.com/1223022): It should be the responsibility of the caller to
  // set `password_issues` to empty.
  // Remove this once all `UpdateLogin` calls have been checked.
  if (password_changed) {
    form_with_encrypted_password.password_issues =
        base::flat_map<InsecureType, InsecurityMetadata>();
  }

  InsecureCredentialsChanged insecure_changed = UpdateInsecureCredentials(
      FormPrimaryKey(old_primary_key_password.primary_key),
      form_with_encrypted_password.password_issues);

  PasswordStoreChangeList list;
  FillFormInStore(&form_with_encrypted_password);
  list.emplace_back(PasswordStoreChange::UPDATE,
                    std::move(form_with_encrypted_password),
                    FormPrimaryKey(old_primary_key_password.primary_key),
                    password_changed, insecure_changed);

  return list;
}

bool LoginDatabase::RemoveLogin(const PasswordForm& form,
                                PasswordStoreChangeList* changes) {
  TRACE_EVENT0("passwords", "LoginDatabase::RemoveLogin");
  if (changes) {
    changes->clear();
  }
  const PrimaryKeyAndPassword old_primary_key_password =
      GetPrimaryKeyAndPassword(form);
#if defined(OS_IOS)
  DeleteEncryptedPasswordFromKeychain(
      old_primary_key_password.encrypted_password);
#endif
  // Remove a login by UNIQUE-constrained fields.
  DCHECK(!delete_statement_.empty());
  sql::Statement s(
      db_.GetCachedStatement(SQL_FROM_HERE, delete_statement_.c_str()));
  s.BindString(0, form.url.spec());
  s.BindString16(1, form.username_element);
  s.BindString16(2, form.username_value);
  s.BindString16(3, form.password_element);
  s.BindString(4, form.signon_realm);

  if (!s.Run() || db_.GetLastChangeCount() == 0) {
    return false;
  }
  if (changes) {
    PasswordForm removed_form = form;
    FillFormInStore(&removed_form);
    changes->emplace_back(PasswordStoreChange::REMOVE, removed_form,
                          FormPrimaryKey(old_primary_key_password.primary_key),
                          /*password_changed=*/true);
  }
  return true;
}

bool LoginDatabase::RemoveLoginByPrimaryKey(FormPrimaryKey primary_key,
                                            PasswordStoreChangeList* changes) {
  TRACE_EVENT0("passwords", "LoginDatabase::RemoveLoginByPrimaryKey");
  PasswordForm form;
  if (changes) {
    changes->clear();
    sql::Statement s1(db_.GetCachedStatement(
        SQL_FROM_HERE, "SELECT * FROM logins WHERE id = ?"));
    s1.BindInt(0, primary_key.value());
    if (!s1.Step()) {
      return false;
    }
    int db_primary_key = -1;
    EncryptionResult result = InitPasswordFormFromStatement(
        s1, /*decrypt_and_fill_password_value=*/false, &db_primary_key, &form);
    DCHECK_EQ(result, ENCRYPTION_RESULT_SUCCESS);
    DCHECK_EQ(db_primary_key, primary_key.value());
  }

#if defined(OS_IOS)
  DeleteEncryptedPasswordById(primary_key.value());
#endif
  DCHECK(!delete_by_id_statement_.empty());
  sql::Statement s2(
      db_.GetCachedStatement(SQL_FROM_HERE, delete_by_id_statement_.c_str()));
  s2.BindInt(0, primary_key.value());
  if (!s2.Run() || db_.GetLastChangeCount() == 0) {
    return false;
  }
  if (changes) {
    FillFormInStore(&form);
    changes->emplace_back(PasswordStoreChange::REMOVE, std::move(form),
                          primary_key,
                          /*password_changed=*/true);
  }
  return true;
}

bool LoginDatabase::RemoveLoginsCreatedBetween(
    base::Time delete_begin,
    base::Time delete_end,
    PasswordStoreChangeList* changes) {
  TRACE_EVENT0("passwords", "LoginDatabase::RemoveLoginsCreatedBetween");
  if (changes) {
    changes->clear();
  }
  PrimaryKeyToFormMap key_to_form_map;
  ScopedTransaction transaction(this);
  if (!GetLoginsCreatedBetween(delete_begin, delete_end, &key_to_form_map)) {
    return false;
  }

#if defined(OS_IOS)
  for (const auto& pair : key_to_form_map) {
    DeleteEncryptedPasswordById(pair.first.value());
  }
#endif

  sql::Statement s(
      db_.GetCachedStatement(SQL_FROM_HERE,
                             "DELETE FROM logins WHERE "
                             "date_created >= ? AND date_created < ?"));
  s.BindInt64(0, delete_begin.ToInternalValue());
  s.BindInt64(1, delete_end.is_null() ? std::numeric_limits<int64_t>::max()
                                      : delete_end.ToInternalValue());

  if (!s.Run()) {
    return false;
  }
  if (changes) {
    for (const auto& pair : key_to_form_map) {
      changes->emplace_back(PasswordStoreChange::REMOVE,
                            /*form=*/std::move(*pair.second),
                            FormPrimaryKey(pair.first),
                            /*password_changed=*/true);
    }
  }
  return true;
}

bool LoginDatabase::GetAutoSignInLogins(PrimaryKeyToFormMap* key_to_form_map) {
  TRACE_EVENT0("passwords", "LoginDatabase::GetAutoSignInLogins");
  DCHECK(key_to_form_map);
  DCHECK(!autosignin_statement_.empty());
  key_to_form_map->clear();

  sql::Statement s(
      db_.GetCachedStatement(SQL_FROM_HERE, autosignin_statement_.c_str()));
  FormRetrievalResult result = StatementToForms(&s, nullptr, key_to_form_map);
  return result == FormRetrievalResult::kSuccess;
}

bool LoginDatabase::DisableAutoSignInForOrigin(const GURL& origin) {
  sql::Statement s(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "UPDATE logins SET skip_zero_click = 1 WHERE origin_url = ?;"));
  s.BindString(0, origin.spec());

  return s.Run();
}

LoginDatabase::EncryptionResult LoginDatabase::InitPasswordFormFromStatement(
    sql::Statement& s,
    bool decrypt_and_fill_password_value,
    int* primary_key,
    PasswordForm* form) const {
  std::string encrypted_password;
  s.ColumnBlobAsString(COLUMN_PASSWORD_VALUE, &encrypted_password);
  std::u16string decrypted_password;
  if (decrypt_and_fill_password_value) {
    EncryptionResult encryption_result =
        DecryptedString(encrypted_password, &decrypted_password);
    if (encryption_result != ENCRYPTION_RESULT_SUCCESS) {
      VLOG(0) << "Password decryption failed, encryption_result is "
              << encryption_result;
      return encryption_result;
    }
  }

  *primary_key = s.ColumnInt(COLUMN_ID);
  std::string tmp = s.ColumnString(COLUMN_ORIGIN_URL);
  form->url = GURL(tmp);
  tmp = s.ColumnString(COLUMN_ACTION_URL);
  form->action = GURL(tmp);
  form->username_element = s.ColumnString16(COLUMN_USERNAME_ELEMENT);
  form->username_value = s.ColumnString16(COLUMN_USERNAME_VALUE);
  form->password_element = s.ColumnString16(COLUMN_PASSWORD_ELEMENT);
  form->password_value = decrypted_password;
  form->encrypted_password = encrypted_password;
  form->submit_element = s.ColumnString16(COLUMN_SUBMIT_ELEMENT);
  tmp = s.ColumnString(COLUMN_SIGNON_REALM);
  form->signon_realm = tmp;
  form->date_created =
      base::Time::FromInternalValue(s.ColumnInt64(COLUMN_DATE_CREATED));
  form->blocked_by_user = (s.ColumnInt(COLUMN_BLOCKLISTED_BY_USER) > 0);
  // TODO(crbug.com/1151214): Add metrics to capture how often these values fall
  // out of the valid enum range.
  form->scheme = static_cast<PasswordForm::Scheme>(s.ColumnInt(COLUMN_SCHEME));
  form->type =
      static_cast<PasswordForm::Type>(s.ColumnInt(COLUMN_PASSWORD_TYPE));
  base::span<const uint8_t> possible_username_pairs_blob =
      s.ColumnBlob(COLUMN_POSSIBLE_USERNAME_PAIRS);
  if (!possible_username_pairs_blob.empty()) {
    base::Pickle pickle = PickleFromSpan(possible_username_pairs_blob);
    form->all_possible_usernames = DeserializeValueElementPairs(pickle);
  }
  form->times_used = s.ColumnInt(COLUMN_TIMES_USED);
  base::span<const uint8_t> form_data_blob = s.ColumnBlob(COLUMN_FORM_DATA);
  if (!form_data_blob.empty()) {
    base::Pickle form_data_pickle = PickleFromSpan(form_data_blob);
    base::PickleIterator form_data_iter(form_data_pickle);
    bool success =
        autofill::DeserializeFormData(&form_data_iter, &form->form_data);
    metrics_util::FormDeserializationStatus status =
        success ? metrics_util::LOGIN_DATABASE_SUCCESS
                : metrics_util::LOGIN_DATABASE_FAILURE;
    metrics_util::LogFormDataDeserializationStatus(status);
  }
  form->display_name = s.ColumnString16(COLUMN_DISPLAY_NAME);
  form->icon_url = GURL(s.ColumnString(COLUMN_ICON_URL));
  form->federation_origin =
      url::Origin::Create(GURL(s.ColumnString(COLUMN_FEDERATION_URL)));
  form->skip_zero_click = (s.ColumnInt(COLUMN_SKIP_ZERO_CLICK) > 0);
  form->generation_upload_status =
      static_cast<PasswordForm::GenerationUploadStatus>(
          s.ColumnInt(COLUMN_GENERATION_UPLOAD_STATUS));
  form->date_last_used = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(s.ColumnInt64(COLUMN_DATE_LAST_USED)));
  base::span<const uint8_t> moving_blocked_for_blob =
      s.ColumnBlob(COLUMN_MOVING_BLOCKED_FOR);
  if (!moving_blocked_for_blob.empty()) {
    base::Pickle pickle = PickleFromSpan(moving_blocked_for_blob);
    form->moving_blocked_for_list = DeserializeGaiaIdHashVector(pickle);
  }
  form->date_password_modified = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(s.ColumnInt64(COLUMN_DATE_PASSWORD_MODIFIED)));
  PopulateFormWithPasswordIssues(FormPrimaryKey(*primary_key), form);

  return ENCRYPTION_RESULT_SUCCESS;
}

bool LoginDatabase::GetLogins(
    const PasswordFormDigest& form,
    bool should_PSL_matching_apply,
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  TRACE_EVENT0("passwords", "LoginDatabase::GetLogins");
  DCHECK(forms);
  forms->clear();

  const bool should_federated_apply =
      form.scheme == PasswordForm::Scheme::kHtml;
  DCHECK(!get_statement_.empty());
  DCHECK(!get_statement_psl_.empty());
  DCHECK(!get_statement_federated_.empty());
  DCHECK(!get_statement_psl_federated_.empty());
  const std::string* sql_query = &get_statement_;
  if (should_PSL_matching_apply && should_federated_apply)
    sql_query = &get_statement_psl_federated_;
  else if (should_PSL_matching_apply)
    sql_query = &get_statement_psl_;
  else if (should_federated_apply)
    sql_query = &get_statement_federated_;

  // TODO(nyquist) Consider usage of GetCachedStatement when
  // http://crbug.com/248608 is fixed.
  sql::Statement s(db_.GetUniqueStatement(sql_query->c_str()));
  s.BindString(0, form.signon_realm);
  int placeholder = 1;

  // PSL matching only applies to HTML forms.
  if (should_PSL_matching_apply) {
    s.BindString(placeholder++, GetRegexForPSLMatching(form.signon_realm));

    if (should_federated_apply) {
      // This regex matches any subdomain of |registered_domain|, in particular
      // it matches the empty subdomain. Hence exact domain matches are also
      // retrieved.
      s.BindString(placeholder++,
                   GetRegexForPSLFederatedMatching(form.signon_realm));
    }
  } else if (should_federated_apply) {
    s.BindString(placeholder++,
                 GetExpressionForFederatedMatching(form.url) + "%");
  }

  PrimaryKeyToFormMap key_to_form_map;
  FormRetrievalResult result = StatementToForms(
      &s, should_PSL_matching_apply || should_federated_apply ? &form : nullptr,
      &key_to_form_map);
  if (result != FormRetrievalResult::kSuccess) {
    return false;
  }
  for (auto& pair : key_to_form_map) {
    forms->push_back(std::move(pair.second));
  }
  return true;
}

bool LoginDatabase::GetLoginsCreatedBetween(
    const base::Time begin,
    const base::Time end,
    PrimaryKeyToFormMap* key_to_form_map) {
  TRACE_EVENT0("passwords", "LoginDatabase::GetLoginsCreatedBetween");
  DCHECK(key_to_form_map);
  DCHECK(!created_statement_.empty());
  sql::Statement s(
      db_.GetCachedStatement(SQL_FROM_HERE, created_statement_.c_str()));
  s.BindInt64(0, begin.ToInternalValue());
  s.BindInt64(1, end.is_null() ? std::numeric_limits<int64_t>::max()
                               : end.ToInternalValue());

  return StatementToForms(&s, nullptr, key_to_form_map) ==
         FormRetrievalResult::kSuccess;
}

FormRetrievalResult LoginDatabase::GetAllLogins(
    PrimaryKeyToFormMap* key_to_form_map) {
  TRACE_EVENT0("passwords", "LoginDatabase::GetAllLogins");
  DCHECK(key_to_form_map);
  key_to_form_map->clear();

  sql::Statement s(
      db_.GetCachedStatement(SQL_FROM_HERE, "SELECT * FROM logins"));

  return StatementToForms(&s, nullptr, key_to_form_map);
}

FormRetrievalResult LoginDatabase::GetLoginsBySignonRealmAndUsername(
    const std::string& signon_realm,
    const std::u16string& username,
    PrimaryKeyToFormMap& key_to_form_map) {
  TRACE_EVENT0("passwords", "LoginDatabase::GetLoginsBySignonRealmAndUsername");
  key_to_form_map.clear();

  sql::Statement s(
      db_.GetCachedStatement(SQL_FROM_HERE, get_statement_username_.c_str()));
  s.BindString(0, signon_realm);
  s.BindString16(1, username);

  return StatementToForms(&s, nullptr, &key_to_form_map);
}

bool LoginDatabase::GetAutofillableLogins(
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  TRACE_EVENT0("passwords", "LoginDatabase::GetAutofillableLogins");
  return GetAllLoginsWithBlocklistSetting(false, forms);
}

bool LoginDatabase::GetBlocklistLogins(
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  TRACE_EVENT0("passwords", "LoginDatabase::GetBlocklistLogins");
  return GetAllLoginsWithBlocklistSetting(true, forms);
}

bool LoginDatabase::GetAllLoginsWithBlocklistSetting(
    bool blocklisted,
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  DCHECK(forms);
  DCHECK(!blocklisted_statement_.empty());
  forms->clear();

  sql::Statement s(
      db_.GetCachedStatement(SQL_FROM_HERE, blocklisted_statement_.c_str()));
  s.BindInt(0, blocklisted ? 1 : 0);

  PrimaryKeyToFormMap key_to_form_map;

  if (StatementToForms(&s, nullptr, &key_to_form_map) !=
      FormRetrievalResult::kSuccess) {
    return false;
  }

  for (auto& pair : key_to_form_map) {
    forms->push_back(std::move(pair.second));
  }
  return true;
}

bool LoginDatabase::IsEmpty() {
  sql::Statement s(
      db_.GetCachedStatement(SQL_FROM_HERE, "SELECT COUNT(*) FROM logins"));
  return s.Step() && s.ColumnInt(0) == 0;
}

bool LoginDatabase::DeleteAndRecreateDatabaseFile() {
  TRACE_EVENT0("passwords", "LoginDatabase::DeleteAndRecreateDatabaseFile");
  DCHECK(db_.is_open());
  meta_table_.Reset();
  db_.Close();
  sql::Database::Delete(db_path_);
  return Init();
}

DatabaseCleanupResult LoginDatabase::DeleteUndecryptableLogins() {
#if defined(OS_MAC)
  TRACE_EVENT0("passwords", "LoginDatabase::DeleteUndecryptableLogins");
  // If the Keychain is unavailable, don't delete any logins.
  if (!OSCrypt::IsEncryptionAvailable()) {
    metrics_util::LogDeleteUndecryptableLoginsReturnValue(
        metrics_util::DeleteCorruptedPasswordsResult::kEncryptionUnavailable);
    return DatabaseCleanupResult::kEncryptionUnavailable;
  }

  DCHECK(db_.is_open());

  sql::Statement s(db_.GetUniqueStatement("SELECT * FROM logins"));

  std::vector<PasswordForm> forms_to_be_deleted;

  while (s.Step()) {
    std::string encrypted_password;
    s.ColumnBlobAsString(COLUMN_PASSWORD_VALUE, &encrypted_password);
    std::u16string decrypted_password;
    if (DecryptedString(encrypted_password, &decrypted_password) ==
        ENCRYPTION_RESULT_SUCCESS)
      continue;

    // If it was not possible to decrypt the password, remove it from the
    // database.
    forms_to_be_deleted.push_back(GetFormForRemoval(s));
  }

  for (const auto& form : forms_to_be_deleted) {
    if (!RemoveLogin(form, nullptr)) {
      metrics_util::LogDeleteUndecryptableLoginsReturnValue(
          metrics_util::DeleteCorruptedPasswordsResult::kItemFailure);
      return DatabaseCleanupResult::kItemFailure;
    }
  }

  if (forms_to_be_deleted.empty()) {
    metrics_util::LogDeleteUndecryptableLoginsReturnValue(
        metrics_util::DeleteCorruptedPasswordsResult::kSuccessNoDeletions);
  } else {
    metrics_util::LogDeleteUndecryptableLoginsReturnValue(
        metrics_util::DeleteCorruptedPasswordsResult::kSuccessPasswordsDeleted);
    UMA_HISTOGRAM_COUNTS_100("PasswordManager.CleanedUpPasswords",
                             forms_to_be_deleted.size());
  }
#endif

  return DatabaseCleanupResult::kSuccess;
}

std::unique_ptr<syncer::MetadataBatch> LoginDatabase::GetAllSyncMetadata() {
  TRACE_EVENT0("passwords", "LoginDatabase::GetAllSyncMetadata");
  std::unique_ptr<syncer::MetadataBatch> metadata_batch =
      GetAllSyncEntityMetadata();
  if (metadata_batch == nullptr) {
    return nullptr;
  }

  std::unique_ptr<sync_pb::ModelTypeState> model_type_state =
      GetModelTypeState();
  if (model_type_state == nullptr) {
    return nullptr;
  }

  metadata_batch->SetModelTypeState(*model_type_state);
  return metadata_batch;
}

void LoginDatabase::DeleteAllSyncMetadata() {
  TRACE_EVENT0("passwords", "LoginDatabase::DeleteAllSyncMetadata");
  bool had_unsynced_deletions = HasUnsyncedDeletions();
  ClearAllSyncMetadata(&db_);
  if (had_unsynced_deletions && deletions_have_synced_callback_) {
    // Note: At this point we can't be fully sure whether the deletions actually
    // reached the server yet. We might have sent a commit, but haven't received
    // the commit confirmation. Let's be conservative and assume they haven't
    // been successfully deleted.
    deletions_have_synced_callback_.Run(/*success=*/false);
  }
}

bool LoginDatabase::UpdateSyncMetadata(
    syncer::ModelType model_type,
    const std::string& storage_key,
    const sync_pb::EntityMetadata& metadata) {
  TRACE_EVENT0("passwords", "LoginDatabase::UpdateSyncMetadata");
  DCHECK_EQ(model_type, syncer::PASSWORDS);

  int storage_key_int = 0;
  if (!base::StringToInt(storage_key, &storage_key_int)) {
    DLOG(ERROR) << "Invalid storage key. Failed to convert the storage key to "
                   "an integer.";
    return false;
  }

  std::string encrypted_metadata;
  if (!OSCrypt::EncryptString(metadata.SerializeAsString(),
                              &encrypted_metadata)) {
    DLOG(ERROR) << "Cannot encrypt the sync metadata";
    return false;
  }

  sql::Statement s(
      db_.GetCachedStatement(SQL_FROM_HERE,
                             "INSERT OR REPLACE INTO sync_entities_metadata "
                             "(storage_key, metadata) VALUES(?, ?)"));

  s.BindInt(0, storage_key_int);
  s.BindString(1, encrypted_metadata);

  bool had_unsynced_deletions = HasUnsyncedDeletions();
  bool result = s.Run();
  if (result && had_unsynced_deletions && !HasUnsyncedDeletions() &&
      deletions_have_synced_callback_) {
    deletions_have_synced_callback_.Run(/*success=*/true);
  }
  return result;
}

bool LoginDatabase::ClearSyncMetadata(syncer::ModelType model_type,
                                      const std::string& storage_key) {
  TRACE_EVENT0("passwords", "LoginDatabase::ClearSyncMetadata");
  DCHECK_EQ(model_type, syncer::PASSWORDS);

  int storage_key_int = 0;
  if (!base::StringToInt(storage_key, &storage_key_int)) {
    DLOG(ERROR) << "Invalid storage key. Failed to convert the storage key to "
                   "an integer.";
    return false;
  }

  sql::Statement s(
      db_.GetCachedStatement(SQL_FROM_HERE,
                             "DELETE FROM sync_entities_metadata WHERE "
                             "storage_key=?"));
  s.BindInt(0, storage_key_int);

  bool had_unsynced_deletions = HasUnsyncedDeletions();
  bool result = s.Run();
  if (result && had_unsynced_deletions && !HasUnsyncedDeletions() &&
      deletions_have_synced_callback_) {
    deletions_have_synced_callback_.Run(/*success=*/true);
  }
  return result;
}

bool LoginDatabase::UpdateModelTypeState(
    syncer::ModelType model_type,
    const sync_pb::ModelTypeState& model_type_state) {
  TRACE_EVENT0("passwords", "LoginDatabase::UpdateModelTypeState");
  DCHECK_EQ(model_type, syncer::PASSWORDS);

  // Make sure only one row is left by storing it in the entry with id=1
  // every time.
  sql::Statement s(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT OR REPLACE INTO sync_model_metadata (id, model_metadata) "
      "VALUES(1, ?)"));
  s.BindString(0, model_type_state.SerializeAsString());

  return s.Run();
}

bool LoginDatabase::ClearModelTypeState(syncer::ModelType model_type) {
  TRACE_EVENT0("passwords", "LoginDatabase::ClearModelTypeState");
  DCHECK_EQ(model_type, syncer::PASSWORDS);

  sql::Statement s(db_.GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM sync_model_metadata WHERE id=1"));

  return s.Run();
}

void LoginDatabase::SetDeletionsHaveSyncedCallback(
    base::RepeatingCallback<void(bool)> callback) {
  deletions_have_synced_callback_ = std::move(callback);
}

bool LoginDatabase::HasUnsyncedDeletions() {
  TRACE_EVENT0("passwords", "LoginDatabase::HasUnsyncedDeletions");

  std::unique_ptr<syncer::MetadataBatch> batch = GetAllSyncEntityMetadata();
  if (!batch)
    return false;
  for (const auto& metadata_entry : batch->GetAllMetadata()) {
    // Note: No need for an explicit "is unsynced" check: Once the deletion is
    // committed, the metadata entry is removed.
    if (metadata_entry.second->is_deleted())
      return true;
  }
  return false;
}

bool LoginDatabase::BeginTransaction() {
  TRACE_EVENT0("passwords", "LoginDatabase::BeginTransaction");
  return db_.BeginTransaction();
}

void LoginDatabase::RollbackTransaction() {
  TRACE_EVENT0("passwords", "LoginDatabase::RollbackTransaction");
  db_.RollbackTransaction();
}

bool LoginDatabase::CommitTransaction() {
  TRACE_EVENT0("passwords", "LoginDatabase::CommitTransaction");
  return db_.CommitTransaction();
}

LoginDatabase::PrimaryKeyAndPassword LoginDatabase::GetPrimaryKeyAndPassword(
    const PasswordForm& form) const {
  DCHECK(!id_and_password_statement_.empty());
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
                                          id_and_password_statement_.c_str()));

  s.BindString(0, form.url.spec());
  s.BindString16(1, form.username_element);
  s.BindString16(2, form.username_value);
  s.BindString16(3, form.password_element);
  s.BindString(4, form.signon_realm);

  if (s.Step()) {
    PrimaryKeyAndPassword result = {s.ColumnInt(0)};
    s.ColumnBlobAsString(1, &result.encrypted_password);
    if (DecryptedString(result.encrypted_password,
                        &result.decrypted_password) !=
        ENCRYPTION_RESULT_SUCCESS) {
      result.decrypted_password.clear();
    }
    return result;
  }
  return {-1, std::string(), std::u16string()};
}

std::unique_ptr<syncer::MetadataBatch>
LoginDatabase::GetAllSyncEntityMetadata() {
  auto metadata_batch = std::make_unique<syncer::MetadataBatch>();
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
                                          "SELECT storage_key, metadata FROM "
                                          "sync_entities_metadata"));

  while (s.Step()) {
    int storage_key_int = s.ColumnInt(0);
    std::string storage_key = base::NumberToString(storage_key_int);
    std::string encrypted_serialized_metadata = s.ColumnString(1);
    std::string decrypted_serialized_metadata;
    if (!OSCrypt::DecryptString(encrypted_serialized_metadata,
                                &decrypted_serialized_metadata)) {
      DLOG(WARNING) << "Failed to decrypt PASSWORD model type "
                       "sync_pb::EntityMetadata.";
      return nullptr;
    }

    auto entity_metadata = std::make_unique<sync_pb::EntityMetadata>();
    if (entity_metadata->ParseFromString(decrypted_serialized_metadata)) {
      metadata_batch->AddMetadata(storage_key, std::move(entity_metadata));
    } else {
      DLOG(WARNING) << "Failed to deserialize PASSWORD model type "
                       "sync_pb::EntityMetadata.";
      return nullptr;
    }
  }
  if (!s.Succeeded()) {
    return nullptr;
  }
  return metadata_batch;
}

std::unique_ptr<sync_pb::ModelTypeState> LoginDatabase::GetModelTypeState() {
  auto state = std::make_unique<sync_pb::ModelTypeState>();
  sql::Statement s(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT model_metadata FROM sync_model_metadata WHERE id=1"));

  if (!s.Step()) {
    if (s.Succeeded())
      return state;
    else
      return nullptr;
  }

  std::string serialized_state = s.ColumnString(0);
  if (state->ParseFromString(serialized_state)) {
    return state;
  }
  return nullptr;
}

FormRetrievalResult LoginDatabase::StatementToForms(
    sql::Statement* statement,
    const PasswordFormDigest* matched_form,
    PrimaryKeyToFormMap* key_to_form_map) {
  key_to_form_map->clear();
  while (statement->Step()) {
    auto new_form = std::make_unique<PasswordForm>();
    FillFormInStore(new_form.get());

    int primary_key = -1;
    EncryptionResult result = InitPasswordFormFromStatement(
        *statement, /*decrypt_and_fill_password_value=*/true, &primary_key,
        new_form.get());
    if (result == ENCRYPTION_RESULT_SERVICE_FAILURE) {
      return FormRetrievalResult::kEncrytionServiceFailure;
    }
    if (result == ENCRYPTION_RESULT_ITEM_FAILURE) {
      continue;
    }
    DCHECK_EQ(ENCRYPTION_RESULT_SUCCESS, result);

    if (matched_form) {
      switch (GetMatchResult(*new_form, *matched_form)) {
        case MatchResult::NO_MATCH:
          continue;
        case MatchResult::EXACT_MATCH:
        case MatchResult::FEDERATED_MATCH:
          break;
        case MatchResult::PSL_MATCH:
        case MatchResult::FEDERATED_PSL_MATCH:
          new_form->is_public_suffix_match = true;
          break;
      }
    }

    key_to_form_map->emplace(primary_key, std::move(new_form));
  }

  if (!statement->Succeeded()) {
    return FormRetrievalResult::kDbError;
  }
  return FormRetrievalResult::kSuccess;
}

void LoginDatabase::InitializeStatementStrings(const SQLTableBuilder& builder) {
  // This method may be called multiple times, if Chrome switches backends and
  // LoginDatabase::DeleteAndRecreateDatabaseFile ends up being called. In those
  // case do not recompute the SQL statements, because they would end up the
  // same.
  if (!add_statement_.empty())
    return;

  // Initialize the cached strings.
  std::string all_column_names = builder.ListAllColumnNames();
  std::string right_amount_of_placeholders =
      GeneratePlaceholders(builder.NumberOfColumns());
  std::string all_unique_key_column_names = builder.ListAllUniqueKeyNames();
  std::string all_nonunique_key_column_names =
      builder.ListAllNonuniqueKeyNames();

  add_statement_ = "INSERT INTO logins (" + all_column_names + ") VALUES " +
                   right_amount_of_placeholders;
  DCHECK(add_replace_statement_.empty());
  add_replace_statement_ = "INSERT OR REPLACE INTO logins (" +
                           all_column_names + ") VALUES " +
                           right_amount_of_placeholders;
  DCHECK(update_statement_.empty());
  update_statement_ = "UPDATE logins SET " + all_nonunique_key_column_names +
                      " WHERE " + all_unique_key_column_names;
  DCHECK(delete_statement_.empty());
  delete_statement_ = "DELETE FROM logins WHERE " + all_unique_key_column_names;
  DCHECK(delete_by_id_statement_.empty());
  delete_by_id_statement_ = "DELETE FROM logins WHERE id=?";
  DCHECK(autosignin_statement_.empty());
  autosignin_statement_ = "SELECT " + all_column_names +
                          " FROM logins "
                          "WHERE skip_zero_click = 0 ORDER BY origin_url";
  DCHECK(get_statement_.empty());
  get_statement_ = "SELECT " + all_column_names +
                   " FROM logins "
                   "WHERE signon_realm == ?";
  std::string psl_statement = "OR signon_realm REGEXP ? ";
  std::string federated_statement =
      "OR (signon_realm LIKE ? AND password_type == 2) ";
  std::string psl_federated_statement =
      "OR (signon_realm REGEXP ? AND password_type == 2) ";
  DCHECK(get_statement_psl_.empty());
  get_statement_psl_ = get_statement_ + psl_statement;
  DCHECK(get_statement_federated_.empty());
  get_statement_federated_ = get_statement_ + federated_statement;
  DCHECK(get_statement_psl_federated_.empty());
  get_statement_psl_federated_ =
      get_statement_ + psl_statement + psl_federated_statement;
  DCHECK(get_statement_username_.empty());
  get_statement_username_ = get_statement_ + " AND username_value == ?";
  DCHECK(created_statement_.empty());
  created_statement_ =
      "SELECT " + all_column_names +
      " FROM logins WHERE date_created >= ? AND date_created < "
      "? ORDER BY origin_url";
  DCHECK(blocklisted_statement_.empty());
  blocklisted_statement_ =
      "SELECT " + all_column_names +
      " FROM logins WHERE blacklisted_by_user == ? ORDER BY origin_url";
  DCHECK(encrypted_password_statement_by_id_.empty());
  encrypted_password_statement_by_id_ =
      "SELECT password_value FROM logins WHERE id=?";
  DCHECK(id_and_password_statement_.empty());
  id_and_password_statement_ = "SELECT id, password_value FROM logins WHERE " +
                               all_unique_key_column_names;
}

void LoginDatabase::FillFormInStore(PasswordForm* form) const {
  form->in_store = is_account_store() ? PasswordForm::Store::kAccountStore
                                      : PasswordForm::Store::kProfileStore;
}

void LoginDatabase::PopulateFormWithPasswordIssues(FormPrimaryKey primary_key,
                                                   PasswordForm* form) const {
  std::vector<InsecureCredential> insecure_credentials =
      insecure_credentials_table_.GetRows(primary_key);
  base::flat_map<InsecureType, InsecurityMetadata> issues;
  for (const auto& insecure_credential : insecure_credentials) {
    issues[insecure_credential.insecure_type] = InsecurityMetadata(
        insecure_credential.create_time, insecure_credential.is_muted);
  }
  form->password_issues = std::move(issues);
}

InsecureCredentialsChanged LoginDatabase::UpdateInsecureCredentials(
    FormPrimaryKey primary_key,
    const base::flat_map<InsecureType, InsecurityMetadata>& password_issues) {
  bool changed = false;
  for (const auto& password_issue : password_issues) {
    changed = insecure_credentials_table_.InsertOrReplace(
                  primary_key, password_issue.first, password_issue.second) ||
              changed;
  }

  // If an insecure type has been removed from the form it has to be removed
  // from the database. This can currently happen for phished entries.
  for (auto insecure_type : {InsecureType::kLeaked, InsecureType::kPhished,
                             InsecureType::kWeak, InsecureType::kReused}) {
    if (password_issues.find(insecure_type) == password_issues.end()) {
      changed =
          insecure_credentials_table_.RemoveRow(primary_key, insecure_type) ||
          changed;
    }
  }
  return InsecureCredentialsChanged(changed);
}

}  // namespace password_manager
