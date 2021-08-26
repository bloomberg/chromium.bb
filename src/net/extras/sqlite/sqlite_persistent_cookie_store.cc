// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/extras/sqlite/sqlite_persistent_cookie_store.h"

#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <tuple>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_util.h"
#include "net/extras/sqlite/cookie_crypto_delegate.h"
#include "net/extras/sqlite/sqlite_persistent_store_backend_base.h"
#include "net/log/net_log.h"
#include "net/log/net_log_values.h"
#include "sql/error_delegate_util.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/third_party/mozilla/url_parse.h"

using base::Time;

namespace {

base::Value CookieKeyedLoadNetLogParams(const std::string& key,
                                        net::NetLogCaptureMode capture_mode) {
  if (!net::NetLogCaptureIncludesSensitive(capture_mode))
    return base::Value();
  base::DictionaryValue dict;
  dict.SetString("key", key);
  return std::move(dict);
}

// Used to populate a histogram for problems when loading cookies.
//
// Please do not reorder or remove entries. New entries must be added to the
// end of the list, just before COOKIE_LOAD_PROBLEM_LAST_ENTRY.
enum CookieLoadProblem {
  COOKIE_LOAD_PROBLEM_DECRYPT_FAILED = 0,
  // Deprecated 03/2021.
  // COOKIE_LOAD_PROBLEM_DECRYPT_TIMEOUT = 1,
  COOKIE_LOAD_PROBLEM_NON_CANONICAL = 2,
  COOKIE_LOAD_PROBLEM_OPEN_DB = 3,
  COOKIE_LOAD_PROBLEM_RECOVERY_FAILED = 4,
  COOKIE_LOAD_PROBLEM_LAST_ENTRY
};

// Used to populate a histogram for problems when committing cookies.
//
// Please do not reorder or remove entries. New entries must be added to the
// end of the list, just before COOKIE_COMMIT_PROBLEM_LAST_ENTRY.
enum CookieCommitProblem {
  COOKIE_COMMIT_PROBLEM_ENCRYPT_FAILED = 0,
  COOKIE_COMMIT_PROBLEM_ADD = 1,
  COOKIE_COMMIT_PROBLEM_UPDATE_ACCESS = 2,
  COOKIE_COMMIT_PROBLEM_DELETE = 3,
  COOKIE_COMMIT_PROBLEM_TRANSACTION_COMMIT = 4,
  COOKIE_COMMIT_PROBLEM_LAST_ENTRY
};

void RecordCookieLoadProblem(CookieLoadProblem event) {
  UMA_HISTOGRAM_ENUMERATION("Cookie.LoadProblem", event,
                            COOKIE_LOAD_PROBLEM_LAST_ENTRY);
}

void RecordCookieCommitProblem(CookieCommitProblem event) {
  UMA_HISTOGRAM_ENUMERATION("Cookie.CommitProblem", event,
                            COOKIE_COMMIT_PROBLEM_LAST_ENTRY);
}

// The persistent cookie store is loaded into memory on eTLD at a time. This
// variable controls the delay between loading eTLDs, so as to not overload the
// CPU or I/O with these low priority requests immediately after start up.
#if defined(OS_IOS)
// TODO(ellyjones): This should be 200ms, but currently CookieStoreIOS is
// waiting for -FinishedLoadingCookies to be called after all eTLD cookies are
// loaded before making any network requests.  Changing to 0ms for now.
// crbug.com/462593
const int kLoadDelayMilliseconds = 0;
#else
const int kLoadDelayMilliseconds = 0;
#endif

// Port number to use for cookies whose source port is unknown at the time of
// database migration to V13. The value -1 comes from url::PORT_UNSPECIFIED.
const int kDefaultUnknownPort = -1;

}  // namespace

namespace net {

base::TaskPriority GetCookieStoreBackgroundSequencePriority() {
  return base::TaskPriority::USER_BLOCKING;
}

namespace {

// Version number of the database.
//
// Version 15 - 2021/07/01 - https://crrev.com/c/3001822
// Version 14 - 2021/02/23 - https://crrev.com/c/2036899
// Version 13 - 2020/10/28 - https://crrev.com/c/2505468
// Version 12 - 2019/11/20 - https://crrev.com/c/1898301
// Version 11 - 2019/04/17 - https://crrev.com/c/1570416
// Version 10 - 2018/02/13 - https://crrev.com/c/906675
// Version 9  - 2015/04/17 - https://codereview.chromium.org/1083623003
//
// Unsupported versions:
// Version 8  - 2015/02/23 - https://codereview.chromium.org/876973003
// Version 7  - 2013/12/16 - https://codereview.chromium.org/24734007
// Version 6  - 2013/04/23 - https://codereview.chromium.org/14208017
// Version 5  - 2011/12/05 - https://codereview.chromium.org/8533013
// Version 4  - 2009/09/01 - https://codereview.chromium.org/183021
//
// Version 15 adds one new field: "top_frame_site_key" (if not empty then the
// string is the scheme and site of the topmost-level frame the cookie was
// created in). This field is deserialized into the cookie's partition key.
// top_frame_site_key is *NOT* the site-for-cookies when the cookie was created.
// In migrating, top_frame_site_key defaults to empty string. This change also
// changes the uniqueness constraint on cookies to include the
// top_frame_site_key as well.
//
// Version 14 just reads all encrypted cookies and re-writes them out again to
// make sure the new encryption key is in use. This active migration only
// happens on Windows, on other OS, this migration is a no-op.
//
// Version 13 adds two new fields: "source_port" (the port number of the source
// origin, and "same_party" (boolean indicating whether the cookie had a
// SameParty attribute). In migrating, source_port defaults to -1
// (url::PORT_UNSPECIFIED) for old entries for which the source port is unknown,
// and same_party defaults to false.
//
// Version 12 adds a column for "source_scheme" to store whether the
// cookie was set from a URL with a cryptographic scheme.
//
// Version 11 renames the "firstpartyonly" column to "samesite", and changes any
// stored values of kCookieSameSiteNoRestriction into
// kCookieSameSiteUnspecified to reflect the fact that those cookies were set
// without a SameSite attribute specified. Support for a value of
// kCookieSameSiteExtended for "samesite" was added, however, that value is now
// deprecated and is mapped to CookieSameSite::UNSPECIFIED when loading from the
// database.
//
// Version 10 removes the uniqueness constraint on the creation time (which
// was not propagated up the stack and caused problems in
// http://crbug.com/800414 and others).  It replaces that constraint by a
// constraint on (name, domain, path), which is spec-compliant (see
// https://tools.ietf.org/html/rfc6265#section-5.3 step 11).  Those fields
// can then be used in place of the creation time for updating access
// time and deleting cookies.
// Version 10 also marks all booleans in the store with an "is_" prefix
// to indicated their booleanness, as SQLite has no such concept.
//
// Version 9 adds a partial index to track non-persistent cookies.
// Non-persistent cookies sometimes need to be deleted on startup. There are
// frequently few or no non-persistent cookies, so the partial index allows the
// deletion to be sped up or skipped, without having to page in the DB.
//
// Version 8 adds "first-party only" cookies.
//
// Version 7 adds encrypted values.  Old values will continue to be used but
// all new values written will be encrypted on selected operating systems.  New
// records read by old clients will simply get an empty cookie value while old
// records read by new clients will continue to operate with the unencrypted
// version.  New and old clients alike will always write/update records with
// what they support.
//
// Version 6 adds cookie priorities. This allows developers to influence the
// order in which cookies are evicted in order to meet domain cookie limits.
//
// Version 5 adds the columns has_expires and is_persistent, so that the
// database can store session cookies as well as persistent cookies. Databases
// of version 5 are incompatible with older versions of code. If a database of
// version 5 is read by older code, session cookies will be treated as normal
// cookies. Currently, these fields are written, but not read anymore.
//
// In version 4, we migrated the time epoch.  If you open the DB with an older
// version on Mac or Linux, the times will look wonky, but the file will likely
// be usable. On Windows version 3 and 4 are the same.
//
// Version 3 updated the database to include the last access time, so we can
// expire them in decreasing order of use when we've reached the maximum
// number of cookies.
const int kCurrentVersionNumber = 15;
const int kCompatibleVersionNumber = 15;

}  // namespace

// This class is designed to be shared between any client thread and the
// background task runner. It batches operations and commits them on a timer.
//
// SQLitePersistentCookieStore::Load is called to load all cookies.  It
// delegates to Backend::Load, which posts a Backend::LoadAndNotifyOnDBThread
// task to the background runner.  This task calls Backend::ChainLoadCookies(),
// which repeatedly posts itself to the BG runner to load each eTLD+1's cookies
// in separate tasks.  When this is complete, Backend::CompleteLoadOnIOThread is
// posted to the client runner, which notifies the caller of
// SQLitePersistentCookieStore::Load that the load is complete.
//
// If a priority load request is invoked via SQLitePersistentCookieStore::
// LoadCookiesForKey, it is delegated to Backend::LoadCookiesForKey, which posts
// Backend::LoadKeyAndNotifyOnDBThread to the BG runner. That routine loads just
// that single domain key (eTLD+1)'s cookies, and posts a Backend::
// CompleteLoadForKeyOnIOThread to the client runner to notify the caller of
// SQLitePersistentCookieStore::LoadCookiesForKey that that load is complete.
//
// Subsequent to loading, mutations may be queued by any thread using
// AddCookie, UpdateCookieAccessTime, and DeleteCookie. These are flushed to
// disk on the BG runner every 30 seconds, 512 operations, or call to Flush(),
// whichever occurs first.
class SQLitePersistentCookieStore::Backend
    : public SQLitePersistentStoreBackendBase {
 public:
  Backend(const base::FilePath& path,
          scoped_refptr<base::SequencedTaskRunner> client_task_runner,
          scoped_refptr<base::SequencedTaskRunner> background_task_runner,
          bool restore_old_session_cookies,
          CookieCryptoDelegate* crypto_delegate)
      : SQLitePersistentStoreBackendBase(path,
                                         /* histogram_tag = */ "Cookie",
                                         kCurrentVersionNumber,
                                         kCompatibleVersionNumber,
                                         std::move(background_task_runner),
                                         std::move(client_task_runner)),
        num_pending_(0),
        restore_old_session_cookies_(restore_old_session_cookies),
        num_priority_waiting_(0),
        total_priority_requests_(0),
        crypto_(crypto_delegate) {}

  // Creates or loads the SQLite database.
  void Load(LoadedCallback loaded_callback);

  // Loads cookies for the domain key (eTLD+1).
  void LoadCookiesForKey(const std::string& domain,
                         LoadedCallback loaded_callback);

  // Steps through all results of |statement|, makes a cookie from each, and
  // adds the cookie to |cookies|. Returns true if everything loaded
  // successfully.
  bool MakeCookiesFromSQLStatement(
      std::vector<std::unique_ptr<CanonicalCookie>>* cookies,
      sql::Statement* statement);

  // Batch a cookie addition.
  void AddCookie(const CanonicalCookie& cc);

  // Batch a cookie access time update.
  void UpdateCookieAccessTime(const CanonicalCookie& cc);

  // Batch a cookie deletion.
  void DeleteCookie(const CanonicalCookie& cc);

  size_t GetQueueLengthForTesting();

  // Post background delete of all cookies that match |cookies|.
  void DeleteAllInList(const std::list<CookieOrigin>& cookies);

 private:
  // You should call Close() before destructing this object.
  ~Backend() override {
    DCHECK_EQ(0u, num_pending_);
    DCHECK(pending_.empty());
  }

  // Database upgrade statements.
  absl::optional<int> DoMigrateDatabaseSchema() override;

  class PendingOperation {
   public:
    enum OperationType {
      COOKIE_ADD,
      COOKIE_UPDATEACCESS,
      COOKIE_DELETE,
    };

    PendingOperation(OperationType op, const CanonicalCookie& cc)
        : op_(op), cc_(cc) {}

    OperationType op() const { return op_; }
    const CanonicalCookie& cc() const { return cc_; }

   private:
    OperationType op_;
    CanonicalCookie cc_;
  };

 private:
  // Creates or loads the SQLite database on background runner.
  void LoadAndNotifyInBackground(LoadedCallback loaded_callback,
                                 const base::Time& posted_at);

  // Loads cookies for the domain key (eTLD+1) on background runner.
  void LoadKeyAndNotifyInBackground(const std::string& domains,
                                    LoadedCallback loaded_callback,
                                    const base::Time& posted_at);

  // Notifies the CookieMonster when loading completes for a specific domain key
  // or for all domain keys. Triggers the callback and passes it all cookies
  // that have been loaded from DB since last IO notification.
  void Notify(LoadedCallback loaded_callback, bool load_success);

  // Sends notification when the entire store is loaded, and reports metrics
  // for the total time to load and aggregated results from any priority loads
  // that occurred.
  void CompleteLoadInForeground(LoadedCallback loaded_callback,
                                bool load_success);

  // Sends notification when a single priority load completes. Updates priority
  // load metric data. The data is sent only after the final load completes.
  void CompleteLoadForKeyInForeground(LoadedCallback loaded_callback,
                                      bool load_success,
                                      const base::Time& requested_at);

  // Sends all metrics, including posting a ReportMetricsInBackground task.
  // Called after all priority and regular loading is complete.
  void ReportMetrics();

  // Sends background-runner owned metrics (i.e., the combined duration of all
  // BG-runner tasks).
  void ReportMetricsInBackground();

  // Initialize the Cookies table.
  bool CreateDatabaseSchema() override;

  // Initialize the data base.
  bool DoInitializeDatabase() override;

  // Loads cookies for the next domain key from the DB, then either reschedules
  // itself or schedules the provided callback to run on the client runner (if
  // all domains are loaded).
  void ChainLoadCookies(LoadedCallback loaded_callback);

  // Load all cookies for a set of domains/hosts. The error recovery code
  // assumes |key| includes all related domains within an eTLD + 1.
  bool LoadCookiesForDomains(const std::set<std::string>& key);

  // Batch a cookie operation (add or delete)
  void BatchOperation(PendingOperation::OperationType op,
                      const CanonicalCookie& cc);
  // Commit our pending operations to the database.
  void DoCommit() override;

  void DeleteSessionCookiesOnStartup();

  void BackgroundDeleteAllInList(const std::list<CookieOrigin>& cookies);

  // Shared code between the different load strategies to be used after all
  // cookies have been loaded.
  void FinishedLoadingCookies(LoadedCallback loaded_callback, bool success);

  void RecordOpenDBProblem() override {
    RecordCookieLoadProblem(COOKIE_LOAD_PROBLEM_OPEN_DB);
  }

  void RecordDBMigrationProblem() override {
    RecordCookieLoadProblem(COOKIE_LOAD_PROBLEM_OPEN_DB);
  }

  typedef std::list<std::unique_ptr<PendingOperation>> PendingOperationsForKey;
  typedef std::map<CanonicalCookie::UniqueCookieKey, PendingOperationsForKey>
      PendingOperationsMap;
  PendingOperationsMap pending_ GUARDED_BY(lock_);
  PendingOperationsMap::size_type num_pending_ GUARDED_BY(lock_);
  // Guard |cookies_|, |pending_|, |num_pending_|.
  base::Lock lock_;

  // Temporary buffer for cookies loaded from DB. Accumulates cookies to reduce
  // the number of messages sent to the client runner. Sent back in response to
  // individual load requests for domain keys or when all loading completes.
  std::vector<std::unique_ptr<CanonicalCookie>> cookies_ GUARDED_BY(lock_);

  // Map of domain keys(eTLD+1) to domains/hosts that are to be loaded from DB.
  std::map<std::string, std::set<std::string>> keys_to_load_;

  // If false, we should filter out session cookies when reading the DB.
  bool restore_old_session_cookies_;

  // The cumulative time spent loading the cookies on the background runner.
  // Incremented and reported from the background runner.
  base::TimeDelta cookie_load_duration_;

  // Guards the following metrics-related properties (only accessed when
  // starting/completing priority loads or completing the total load).
  base::Lock metrics_lock_;
  int num_priority_waiting_ GUARDED_BY(metrics_lock_);
  // The total number of priority requests.
  int total_priority_requests_ GUARDED_BY(metrics_lock_);
  // The time when |num_priority_waiting_| incremented to 1.
  base::Time current_priority_wait_start_ GUARDED_BY(metrics_lock_);
  // The cumulative duration of time when |num_priority_waiting_| was greater
  // than 1.
  base::TimeDelta priority_wait_duration_ GUARDED_BY(metrics_lock_);
  // Class with functions that do cryptographic operations (for protecting
  // cookies stored persistently).
  //
  // Not owned.
  CookieCryptoDelegate* crypto_;

  DISALLOW_COPY_AND_ASSIGN(Backend);
};

namespace {

// Possible values for the 'priority' column.
enum DBCookiePriority {
  kCookiePriorityLow = 0,
  kCookiePriorityMedium = 1,
  kCookiePriorityHigh = 2,
};

DBCookiePriority CookiePriorityToDBCookiePriority(CookiePriority value) {
  switch (value) {
    case COOKIE_PRIORITY_LOW:
      return kCookiePriorityLow;
    case COOKIE_PRIORITY_MEDIUM:
      return kCookiePriorityMedium;
    case COOKIE_PRIORITY_HIGH:
      return kCookiePriorityHigh;
  }

  NOTREACHED();
  return kCookiePriorityMedium;
}

CookiePriority DBCookiePriorityToCookiePriority(DBCookiePriority value) {
  switch (value) {
    case kCookiePriorityLow:
      return COOKIE_PRIORITY_LOW;
    case kCookiePriorityMedium:
      return COOKIE_PRIORITY_MEDIUM;
    case kCookiePriorityHigh:
      return COOKIE_PRIORITY_HIGH;
  }

  NOTREACHED();
  return COOKIE_PRIORITY_DEFAULT;
}

// Possible values for the 'samesite' column
enum DBCookieSameSite {
  kCookieSameSiteUnspecified = -1,
  kCookieSameSiteNoRestriction = 0,
  kCookieSameSiteLax = 1,
  kCookieSameSiteStrict = 2,
  // Deprecated, mapped to kCookieSameSiteUnspecified.
  kCookieSameSiteExtended = 3
};

DBCookieSameSite CookieSameSiteToDBCookieSameSite(CookieSameSite value) {
  switch (value) {
    case CookieSameSite::NO_RESTRICTION:
      return kCookieSameSiteNoRestriction;
    case CookieSameSite::LAX_MODE:
      return kCookieSameSiteLax;
    case CookieSameSite::STRICT_MODE:
      return kCookieSameSiteStrict;
    case CookieSameSite::UNSPECIFIED:
      return kCookieSameSiteUnspecified;
  }
}

CookieSameSite DBCookieSameSiteToCookieSameSite(DBCookieSameSite value) {
  CookieSameSite samesite = CookieSameSite::UNSPECIFIED;
  switch (value) {
    case kCookieSameSiteNoRestriction:
      samesite = CookieSameSite::NO_RESTRICTION;
      break;
    case kCookieSameSiteLax:
      samesite = CookieSameSite::LAX_MODE;
      break;
    case kCookieSameSiteStrict:
      samesite = CookieSameSite::STRICT_MODE;
      break;
    // SameSite=Extended is deprecated, so we map to UNSPECIFIED.
    case kCookieSameSiteExtended:
    case kCookieSameSiteUnspecified:
      samesite = CookieSameSite::UNSPECIFIED;
      break;
  }
  return samesite;
}

CookieSourceScheme DBToCookieSourceScheme(int value) {
  int enum_max_value = static_cast<int>(CookieSourceScheme::kMaxValue);

  if (value < 0 || value > enum_max_value) {
    DLOG(WARNING) << "DB read of cookie's source scheme is invalid. Resetting "
                     "value to unset.";
    value = static_cast<int>(
        CookieSourceScheme::kUnset);  // Reset value to a known, useful, state.
  }

  return static_cast<CookieSourceScheme>(value);
}

// Increments a specified TimeDelta by the duration between this object's
// constructor and destructor. Not thread safe. Multiple instances may be
// created with the same delta instance as long as their lifetimes are nested.
// The shortest lived instances have no impact.
class IncrementTimeDelta {
 public:
  explicit IncrementTimeDelta(base::TimeDelta* delta)
      : delta_(delta), original_value_(*delta), start_(base::Time::Now()) {}

  ~IncrementTimeDelta() {
    *delta_ = original_value_ + base::Time::Now() - start_;
  }

 private:
  base::TimeDelta* delta_;
  base::TimeDelta original_value_;
  base::Time start_;

  DISALLOW_COPY_AND_ASSIGN(IncrementTimeDelta);
};

// Initializes the cookies table, returning true on success.
// The table cannot exist when calling this function.
bool CreateV10Schema(sql::Database* db) {
  DCHECK(!db->DoesTableExist("cookies"));

  std::string stmt(base::StringPrintf(
      "CREATE TABLE cookies("
      "creation_utc INTEGER NOT NULL,"
      "host_key TEXT NOT NULL,"
      "name TEXT NOT NULL,"
      "value TEXT NOT NULL,"
      "path TEXT NOT NULL,"
      "expires_utc INTEGER NOT NULL,"
      "is_secure INTEGER NOT NULL,"
      "is_httponly INTEGER NOT NULL,"
      "last_access_utc INTEGER NOT NULL,"
      "has_expires INTEGER NOT NULL DEFAULT 1,"
      "is_persistent INTEGER NOT NULL DEFAULT 1,"
      "priority INTEGER NOT NULL DEFAULT %d,"
      "encrypted_value BLOB DEFAULT '',"
      "firstpartyonly INTEGER NOT NULL DEFAULT %d,"
      "UNIQUE (host_key, name, path))",
      CookiePriorityToDBCookiePriority(COOKIE_PRIORITY_DEFAULT),
      CookieSameSiteToDBCookieSameSite(CookieSameSite::NO_RESTRICTION)));
  if (!db->Execute(stmt.c_str()))
    return false;

  return true;
}

// Initializes the cookies table, returning true on success.
// The table cannot exist when calling this function.
bool CreateV11Schema(sql::Database* db) {
  DCHECK(!db->DoesTableExist("cookies"));

  std::string stmt(base::StringPrintf(
      "CREATE TABLE cookies("
      "creation_utc INTEGER NOT NULL,"
      "host_key TEXT NOT NULL,"
      "name TEXT NOT NULL,"
      "value TEXT NOT NULL,"
      "path TEXT NOT NULL,"
      "expires_utc INTEGER NOT NULL,"
      "is_secure INTEGER NOT NULL,"
      "is_httponly INTEGER NOT NULL,"
      "last_access_utc INTEGER NOT NULL,"
      "has_expires INTEGER NOT NULL DEFAULT 1,"
      "is_persistent INTEGER NOT NULL DEFAULT 1,"
      "priority INTEGER NOT NULL DEFAULT %d,"
      "encrypted_value BLOB DEFAULT '',"
      "samesite INTEGER NOT NULL DEFAULT %d,"
      "UNIQUE (host_key, name, path))",
      CookiePriorityToDBCookiePriority(COOKIE_PRIORITY_DEFAULT),
      CookieSameSiteToDBCookieSameSite(CookieSameSite::UNSPECIFIED)));
  if (!db->Execute(stmt.c_str()))
    return false;

  return true;
}

// Initializes the cookies table, returning true on success.
// The table cannot exist when calling this function.
bool CreateV15Schema(sql::Database* db) {
  DCHECK(!db->DoesTableExist("cookies"));

  std::string stmt(base::StringPrintf(
      "CREATE TABLE cookies("
      "creation_utc INTEGER NOT NULL,"
      "top_frame_site_key TEXT NOT NULL,"
      "host_key TEXT NOT NULL,"
      "name TEXT NOT NULL,"
      "value TEXT NOT NULL,"
      "encrypted_value BLOB DEFAULT '',"
      "path TEXT NOT NULL,"
      "expires_utc INTEGER NOT NULL,"
      "is_secure INTEGER NOT NULL,"
      "is_httponly INTEGER NOT NULL,"
      "last_access_utc INTEGER NOT NULL,"
      "has_expires INTEGER NOT NULL DEFAULT 1,"
      "is_persistent INTEGER NOT NULL DEFAULT 1,"
      "priority INTEGER NOT NULL DEFAULT %d,"
      "samesite INTEGER NOT NULL DEFAULT %d,"
      "source_scheme INTEGER NOT NULL DEFAULT %d,"
      "source_port INTEGER NOT NULL DEFAULT %d,"
      "is_same_party INTEGER NOT NULL DEFAULT 0,"
      "UNIQUE (top_frame_site_key, host_key, name, path))",
      CookiePriorityToDBCookiePriority(COOKIE_PRIORITY_DEFAULT),
      CookieSameSiteToDBCookieSameSite(CookieSameSite::UNSPECIFIED),
      static_cast<int>(CookieSourceScheme::kUnset), kDefaultUnknownPort));
  if (!db->Execute(stmt.c_str()))
    return false;

  return true;
}

}  // namespace

void SQLitePersistentCookieStore::Backend::Load(
    LoadedCallback loaded_callback) {
  PostBackgroundTask(
      FROM_HERE, base::BindOnce(&Backend::LoadAndNotifyInBackground, this,
                                std::move(loaded_callback), base::Time::Now()));
}

void SQLitePersistentCookieStore::Backend::LoadCookiesForKey(
    const std::string& key,
    LoadedCallback loaded_callback) {
  {
    base::AutoLock locked(metrics_lock_);
    if (num_priority_waiting_ == 0)
      current_priority_wait_start_ = base::Time::Now();
    num_priority_waiting_++;
    total_priority_requests_++;
  }

  PostBackgroundTask(
      FROM_HERE,
      base::BindOnce(&Backend::LoadKeyAndNotifyInBackground, this, key,
                     std::move(loaded_callback), base::Time::Now()));
}

void SQLitePersistentCookieStore::Backend::LoadAndNotifyInBackground(
    LoadedCallback loaded_callback,
    const base::Time& posted_at) {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  IncrementTimeDelta increment(&cookie_load_duration_);

  UMA_HISTOGRAM_CUSTOM_TIMES("Cookie.TimeLoadDBQueueWait",
                             base::Time::Now() - posted_at,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMinutes(1), 50);

  if (!InitializeDatabase()) {
    PostClientTask(FROM_HERE,
                   base::BindOnce(&Backend::CompleteLoadInForeground, this,
                                  std::move(loaded_callback), false));
  } else {
    ChainLoadCookies(std::move(loaded_callback));
  }
}

void SQLitePersistentCookieStore::Backend::LoadKeyAndNotifyInBackground(
    const std::string& key,
    LoadedCallback loaded_callback,
    const base::Time& posted_at) {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  IncrementTimeDelta increment(&cookie_load_duration_);

  UMA_HISTOGRAM_CUSTOM_TIMES("Cookie.TimeKeyLoadDBQueueWait",
                             base::Time::Now() - posted_at,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMinutes(1), 50);

  bool success = false;
  if (InitializeDatabase()) {
    auto it = keys_to_load_.find(key);
    if (it != keys_to_load_.end()) {
      success = LoadCookiesForDomains(it->second);
      keys_to_load_.erase(it);
    } else {
      success = true;
    }
  }

  PostClientTask(
      FROM_HERE,
      base::BindOnce(
          &SQLitePersistentCookieStore::Backend::CompleteLoadForKeyInForeground,
          this, std::move(loaded_callback), success, posted_at));
}

void SQLitePersistentCookieStore::Backend::CompleteLoadForKeyInForeground(
    LoadedCallback loaded_callback,
    bool load_success,
    const ::Time& requested_at) {
  DCHECK(client_task_runner()->RunsTasksInCurrentSequence());

  UMA_HISTOGRAM_CUSTOM_TIMES("Cookie.TimeKeyLoadTotalWait",
                             base::Time::Now() - requested_at,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMinutes(1), 50);

  Notify(std::move(loaded_callback), load_success);

  {
    base::AutoLock locked(metrics_lock_);
    num_priority_waiting_--;
    if (num_priority_waiting_ == 0) {
      priority_wait_duration_ +=
          base::Time::Now() - current_priority_wait_start_;
    }
  }
}

void SQLitePersistentCookieStore::Backend::ReportMetricsInBackground() {
  UMA_HISTOGRAM_CUSTOM_TIMES("Cookie.TimeLoad", cookie_load_duration_,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMinutes(1), 50);
}

void SQLitePersistentCookieStore::Backend::ReportMetrics() {
  PostBackgroundTask(
      FROM_HERE,
      base::BindOnce(
          &SQLitePersistentCookieStore::Backend::ReportMetricsInBackground,
          this));

  {
    base::AutoLock locked(metrics_lock_);
    UMA_HISTOGRAM_CUSTOM_TIMES("Cookie.PriorityBlockingTime",
                               priority_wait_duration_,
                               base::TimeDelta::FromMilliseconds(1),
                               base::TimeDelta::FromMinutes(1), 50);

    UMA_HISTOGRAM_COUNTS_100("Cookie.PriorityLoadCount",
                             total_priority_requests_);
  }
}

void SQLitePersistentCookieStore::Backend::CompleteLoadInForeground(
    LoadedCallback loaded_callback,
    bool load_success) {
  Notify(std::move(loaded_callback), load_success);

  if (load_success)
    ReportMetrics();
}

void SQLitePersistentCookieStore::Backend::Notify(
    LoadedCallback loaded_callback,
    bool load_success) {
  DCHECK(client_task_runner()->RunsTasksInCurrentSequence());

  std::vector<std::unique_ptr<CanonicalCookie>> cookies;
  {
    base::AutoLock locked(lock_);
    cookies.swap(cookies_);
  }

  std::move(loaded_callback).Run(std::move(cookies));
}

bool SQLitePersistentCookieStore::Backend::CreateDatabaseSchema() {
  DCHECK(db());

  if (db()->DoesTableExist("cookies"))
    return true;

  return CreateV15Schema(db());
}

bool SQLitePersistentCookieStore::Backend::DoInitializeDatabase() {
  DCHECK(db());

  // Retrieve all the domains
  sql::Statement smt(
      db()->GetUniqueStatement("SELECT DISTINCT host_key FROM cookies"));

  if (!smt.is_valid()) {
    Reset();
    return false;
  }

  std::vector<std::string> host_keys;
  while (smt.Step())
    host_keys.push_back(smt.ColumnString(0));

  // Build a map of domain keys (always eTLD+1) to domains.
  for (size_t idx = 0; idx < host_keys.size(); ++idx) {
    const std::string& domain = host_keys[idx];
    std::string key = CookieMonster::GetKey(domain);
    keys_to_load_[key].insert(domain);
  }

  if (!restore_old_session_cookies_)
    DeleteSessionCookiesOnStartup();

  return true;
}

void SQLitePersistentCookieStore::Backend::ChainLoadCookies(
    LoadedCallback loaded_callback) {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  IncrementTimeDelta increment(&cookie_load_duration_);

  bool load_success = true;

  if (!db()) {
    // Close() has been called on this store.
    load_success = false;
  } else if (keys_to_load_.size() > 0) {
    // Load cookies for the first domain key.
    auto it = keys_to_load_.begin();
    load_success = LoadCookiesForDomains(it->second);
    keys_to_load_.erase(it);
  }

  // If load is successful and there are more domain keys to be loaded,
  // then post a background task to continue chain-load;
  // Otherwise notify on client runner.
  if (load_success && keys_to_load_.size() > 0) {
    bool success = background_task_runner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&Backend::ChainLoadCookies, this,
                       std::move(loaded_callback)),
        base::TimeDelta::FromMilliseconds(kLoadDelayMilliseconds));
    if (!success) {
      LOG(WARNING) << "Failed to post task from " << FROM_HERE.ToString()
                   << " to background_task_runner().";
    }
  } else {
    FinishedLoadingCookies(std::move(loaded_callback), load_success);
  }
}

bool SQLitePersistentCookieStore::Backend::LoadCookiesForDomains(
    const std::set<std::string>& domains) {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());

  // TODO(crbug.com/1225444) Query top_frame_site_key from database.
  sql::Statement smt, delete_statement;
  if (restore_old_session_cookies_) {
    smt.Assign(db()->GetCachedStatement(
        SQL_FROM_HERE,
        "SELECT creation_utc, host_key, name, value, path, expires_utc, "
        "is_secure, is_httponly, last_access_utc, has_expires, is_persistent, "
        "priority, encrypted_value, samesite, source_scheme, source_port, "
        "is_same_party FROM cookies WHERE top_frame_site_key = ? AND "
        "host_key = ?"));
  } else {
    smt.Assign(db()->GetCachedStatement(
        SQL_FROM_HERE,
        "SELECT creation_utc, host_key, name, value, path, expires_utc, "
        "is_secure, is_httponly, last_access_utc, has_expires, is_persistent, "
        "priority, encrypted_value, samesite, source_scheme, source_port, "
        "is_same_party FROM cookies WHERE top_frame_site_key = ? AND "
        "host_key = ? AND is_persistent = 1"));
  }
  delete_statement.Assign(db()->GetCachedStatement(
      SQL_FROM_HERE,
      "DELETE FROM cookies WHERE top_frame_site_key = ? AND host_key = ?"));
  if (!smt.is_valid() || !delete_statement.is_valid()) {
    delete_statement.Clear();
    smt.Clear();  // Disconnect smt_ref from db_.
    Reset();
    return false;
  }

  std::vector<std::unique_ptr<CanonicalCookie>> cookies;
  auto it = domains.begin();
  bool ok = true;
  for (; it != domains.end() && ok; ++it) {
    smt.BindString(0, net::kEmptyCookiePartitionKey);
    smt.BindString(1, *it);
    ok = MakeCookiesFromSQLStatement(&cookies, &smt);
    smt.Reset(true);
  }

  if (ok) {
    base::AutoLock locked(lock_);
    std::move(cookies.begin(), cookies.end(), std::back_inserter(cookies_));
  } else {
    // There were some cookies that were in database but could not be loaded
    // and handed over to CookieMonster. This is trouble since it means that
    // if some website tries to send them again, CookieMonster won't know to
    // issue a delete, and then the addition would violate the uniqueness
    // constraints and not go through.
    //
    // For data consistency, we drop the entire eTLD group.
    for (const std::string& domain : domains) {
      delete_statement.BindString(0, net::kEmptyCookiePartitionKey);
      delete_statement.BindString(1, domain);
      if (!delete_statement.Run()) {
        // TODO(morlovich): Is something more drastic called for here?
        RecordCookieLoadProblem(COOKIE_LOAD_PROBLEM_RECOVERY_FAILED);
      }
      delete_statement.Reset(true);
    }
  }
  return true;
}

bool SQLitePersistentCookieStore::Backend::MakeCookiesFromSQLStatement(
    std::vector<std::unique_ptr<CanonicalCookie>>* cookies,
    sql::Statement* statement) {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  sql::Statement& smt = *statement;
  bool ok = true;
  while (smt.Step()) {
    std::string value;
    std::string encrypted_value = smt.ColumnString(12);
    if (!encrypted_value.empty() && crypto_) {
      bool decrypt_ok = crypto_->DecryptString(encrypted_value, &value);
      if (!decrypt_ok) {
        RecordCookieLoadProblem(COOKIE_LOAD_PROBLEM_DECRYPT_FAILED);
        ok = false;
        continue;
      }
    } else {
      value = smt.ColumnString(3);
    }
    // Returns nullptr if the resulting cookie is not canonical.
    std::unique_ptr<net::CanonicalCookie> cc = CanonicalCookie::FromStorage(
        smt.ColumnString(2),  // name
        value,                // value
        smt.ColumnString(1),  // domain
        smt.ColumnString(4),  // path
        smt.ColumnTime(0),    // creation_utc
        smt.ColumnTime(5),    // expires_utc
        smt.ColumnTime(8),    // last_access_utc
        smt.ColumnBool(6),    // secure
        smt.ColumnBool(7),    // http_only
        DBCookieSameSiteToCookieSameSite(
            static_cast<DBCookieSameSite>(smt.ColumnInt(13))),  // samesite
        DBCookiePriorityToCookiePriority(
            static_cast<DBCookiePriority>(smt.ColumnInt(11))),  // priority
        smt.ColumnBool(16),                                     // is_same_party
        // TODO(crbug.com/1225444) Deserialize top_frame_site_key into the
        // cookie partition key.
        absl::nullopt,                              // top_frame_site_key
        DBToCookieSourceScheme(smt.ColumnInt(14)),  // source_scheme
        smt.ColumnInt(15));                         // source_port
    if (cc) {
      DLOG_IF(WARNING, cc->CreationDate() > Time::Now())
          << L"CreationDate too recent";
      cookies->push_back(std::move(cc));
    } else {
      RecordCookieLoadProblem(COOKIE_LOAD_PROBLEM_NON_CANONICAL);
      ok = false;
    }
  }

  return ok;
}

absl::optional<int>
SQLitePersistentCookieStore::Backend::DoMigrateDatabaseSchema() {
  int cur_version = meta_table()->GetVersionNumber();
  if (cur_version == 9) {
    const base::TimeTicks start_time = base::TimeTicks::Now();
    sql::Transaction transaction(db());
    if (!transaction.Begin())
      return absl::nullopt;

    if (!db()->Execute("ALTER TABLE cookies RENAME TO cookies_old"))
      return absl::nullopt;
    if (!db()->Execute("DROP INDEX IF EXISTS domain"))
      return absl::nullopt;
    if (!db()->Execute("DROP INDEX IF EXISTS is_transient"))
      return absl::nullopt;

    if (!CreateV10Schema(db())) {
      // Not clear what good a false return here will do since the calling
      // code will just init the table.
      // TODO(rdsmith): Also, wait, nothing drops the old table and
      // InitTable() just returns true if the table exists, so if
      // EnsureDatabaseVersion() fails, initting the table won't do any
      // further good.  Fix?
      return absl::nullopt;
    }
    // If any cookies violate the new uniqueness constraints (no two
    // cookies with the same (name, domain, path)), pick the newer version,
    // since that's what CookieMonster would do anyway.
    if (!db()->Execute(
            "INSERT OR REPLACE INTO cookies "
            "(creation_utc, host_key, name, value, path, expires_utc, "
            "is_secure, is_httponly, last_access_utc, has_expires, "
            "is_persistent, priority, encrypted_value, firstpartyonly) "
            "SELECT creation_utc, host_key, name, value, path, expires_utc, "
            "       secure, httponly, last_access_utc, has_expires, "
            "       persistent, priority, encrypted_value, firstpartyonly "
            "FROM cookies_old ORDER BY creation_utc ASC")) {
      return absl::nullopt;
    }
    if (!db()->Execute("DROP TABLE cookies_old"))
      return absl::nullopt;
    ++cur_version;
    meta_table()->SetVersionNumber(cur_version);
    meta_table()->SetCompatibleVersionNumber(
        std::min(cur_version, kCompatibleVersionNumber));
    transaction.Commit();
    base::UmaHistogramTimes("Cookie.TimeDatabaseMigrationToV10",
                            base::TimeTicks::Now() - start_time);
  }

  if (cur_version == 10) {
    sql::Transaction transaction(db());
    if (!transaction.Begin())
      return absl::nullopt;

    // Copy the data into a new table, renaming the firstpartyonly column to
    // samesite.
    if (!db()->Execute("DROP TABLE IF EXISTS cookies_old; "
                       "ALTER TABLE cookies RENAME TO cookies_old"))
      return absl::nullopt;
    if (!CreateV11Schema(db()))
      return absl::nullopt;
    if (!db()->Execute(
            "INSERT INTO cookies "
            "(creation_utc, host_key, name, value, path, expires_utc, "
            "is_secure, is_httponly, last_access_utc, has_expires, "
            "is_persistent, priority, encrypted_value, samesite) "
            "SELECT creation_utc, host_key, name, value, path, expires_utc, "
            "       is_secure, is_httponly, last_access_utc, has_expires, "
            "       is_persistent, priority, encrypted_value, firstpartyonly "
            "FROM cookies_old")) {
      return absl::nullopt;
    }
    if (!db()->Execute("DROP TABLE cookies_old"))
      return absl::nullopt;

    // Update stored SameSite values of kCookieSameSiteNoRestriction into
    // kCookieSameSiteUnspecified.
    std::string update_stmt(base::StringPrintf(
        "UPDATE cookies SET samesite=%d WHERE samesite=%d",
        CookieSameSiteToDBCookieSameSite(CookieSameSite::UNSPECIFIED),
        CookieSameSiteToDBCookieSameSite(CookieSameSite::NO_RESTRICTION)));
    if (!db()->Execute(update_stmt.c_str()))
      return absl::nullopt;

    ++cur_version;
    meta_table()->SetVersionNumber(cur_version);
    meta_table()->SetCompatibleVersionNumber(
        std::min(cur_version, kCompatibleVersionNumber));
    transaction.Commit();
  }

  if (cur_version == 11) {
    SCOPED_UMA_HISTOGRAM_TIMER("Cookie.TimeDatabaseMigrationToV12");
    sql::Transaction transaction(db());
    if (!transaction.Begin())
      return absl::nullopt;

    std::string update_stmt(
        base::StringPrintf("ALTER TABLE cookies ADD COLUMN source_scheme "
                           "INTEGER NOT NULL DEFAULT %d;",
                           static_cast<int>(CookieSourceScheme::kUnset)));
    if (!db()->Execute(update_stmt.c_str()))
      return absl::nullopt;

    ++cur_version;
    meta_table()->SetVersionNumber(cur_version);
    meta_table()->SetCompatibleVersionNumber(
        std::min(cur_version, kCompatibleVersionNumber));
    transaction.Commit();
  }

  if (cur_version == 12) {
    const char kMigrationSuccessHistogram[] =
        "Cookie.TimeDatabaseMigrationToV13Success";
    const char kMigrationFailureHistogram[] =
        "Cookie.TimeDatabaseMigrationToV13Failure";
    const base::TimeTicks start_time = base::TimeTicks::Now();

    sql::Transaction transaction(db());
    if (!transaction.Begin()) {
      base::UmaHistogramTimes(kMigrationFailureHistogram,
                              base::TimeTicks::Now() - start_time);
      return absl::nullopt;
    }

    std::string update_stmt(
        base::StringPrintf("ALTER TABLE cookies ADD COLUMN source_port "
                           "INTEGER NOT NULL DEFAULT %d;"
                           "ALTER TABLE cookies ADD COLUMN is_same_party "
                           "INTEGER NOT NULL DEFAULT 0;",
                           kDefaultUnknownPort));
    if (!db()->Execute(update_stmt.c_str())) {
      base::UmaHistogramTimes(kMigrationFailureHistogram,
                              base::TimeTicks::Now() - start_time);
      return absl::nullopt;
    }

    ++cur_version;
    meta_table()->SetVersionNumber(cur_version);
    meta_table()->SetCompatibleVersionNumber(
        std::min(cur_version, kCompatibleVersionNumber));
    transaction.Commit();
    base::UmaHistogramTimes(kMigrationSuccessHistogram,
                            base::TimeTicks::Now() - start_time);
  }

  if (cur_version == 13) {
    const base::TimeTicks start_time = base::TimeTicks::Now();
    sql::Transaction transaction(db());
    if (!transaction.Begin())
      return absl::nullopt;

#if defined(OS_WIN)
    // Migration is only needed on Windows. On other platforms, this is a no-op.
    if (crypto_ && crypto_->ShouldEncrypt()) {
      sql::Statement select_statement, update_statement;

      select_statement.Assign(
          db()->GetCachedStatement(SQL_FROM_HERE,
                                   "SELECT rowid, encrypted_value "
                                   "FROM cookies WHERE encrypted_value != ''"));

      update_statement.Assign(
          db()->GetCachedStatement(SQL_FROM_HERE,
                                   "UPDATE cookies SET encrypted_value=? WHERE "
                                   "rowid=?"));

      if (!select_statement.is_valid() || !update_statement.is_valid())
        return absl::nullopt;

      bool okay = true;

      std::map<int64_t, std::string> encrypted_values;

      while (select_statement.Step()) {
        int64_t rowid = select_statement.ColumnInt64(0);
        std::string encrypted_value = select_statement.ColumnString(1);
        DCHECK(!encrypted_value.empty());
        std::string decrypted_value;
        if (!crypto_->DecryptString(encrypted_value, &decrypted_value)) {
          RecordCookieLoadProblem(COOKIE_LOAD_PROBLEM_DECRYPT_FAILED);
          okay = false;
          continue;
        }
        std::string new_encrypted_value;
        if (!crypto_->EncryptString(decrypted_value, &new_encrypted_value)) {
          RecordCookieCommitProblem(COOKIE_COMMIT_PROBLEM_ENCRYPT_FAILED);
          okay = false;
          continue;
        }
        encrypted_values[rowid] = new_encrypted_value;
      }

      for (const auto& entry : encrypted_values) {
        update_statement.Reset(true);
        update_statement.BindString(0, entry.second);
        update_statement.BindInt64(1, entry.first);
        if (!update_statement.Run())
          return absl::nullopt;
      }

      UMA_HISTOGRAM_BOOLEAN("Cookie.MigratedEncryptionKeySuccess", okay);
    }
#endif
    ++cur_version;
    meta_table()->SetVersionNumber(cur_version);
    meta_table()->SetCompatibleVersionNumber(
        std::min(cur_version, kCompatibleVersionNumber));
    transaction.Commit();
    base::UmaHistogramTimes("Cookie.TimeDatabaseMigrationToV14",
                            base::TimeTicks::Now() - start_time);
  }

  if (cur_version == 14) {
    SCOPED_UMA_HISTOGRAM_TIMER("Cookie.TimeDatabaseMigrationToV15");

    sql::Transaction transaction(db());
    if (!transaction.Begin())
      return absl::nullopt;

    if (!db()->Execute("DROP TABLE IF EXISTS cookies_old"))
      return absl::nullopt;
    if (!db()->Execute("ALTER TABLE cookies RENAME TO cookies_old"))
      return absl::nullopt;

    if (!CreateV15Schema(db()))
      return absl::nullopt;
    std::string insert_cookies_sql = base::StringPrintf(
        "INSERT OR REPLACE INTO cookies "
        "(creation_utc, top_frame_site_key, host_key, name, value, "
        " encrypted_value, path, expires_utc, is_secure, is_httponly, "
        " last_access_utc, has_expires, is_persistent, priority, samesite, "
        " source_scheme, source_port, is_same_party) "
        "SELECT creation_utc, '%s', host_key, name, value, encrypted_value, "
        "       path, expires_utc, is_secure, is_httponly, last_access_utc, "
        "       has_expires, is_persistent, priority, samesite, source_scheme, "
        "       source_port, is_same_party "
        "FROM cookies_old ORDER BY creation_utc ASC",
        net::kEmptyCookiePartitionKey);
    if (!db()->Execute(insert_cookies_sql.c_str()))
      return absl::nullopt;
    if (!db()->Execute("DROP TABLE cookies_old"))
      return absl::nullopt;

    ++cur_version;
    meta_table()->SetVersionNumber(cur_version);
    meta_table()->SetCompatibleVersionNumber(
        std::min(cur_version, kCompatibleVersionNumber));
    transaction.Commit();
  }

  // Put future migration cases here.

  return absl::make_optional(cur_version);
}

void SQLitePersistentCookieStore::Backend::AddCookie(
    const CanonicalCookie& cc) {
  BatchOperation(PendingOperation::COOKIE_ADD, cc);
}

void SQLitePersistentCookieStore::Backend::UpdateCookieAccessTime(
    const CanonicalCookie& cc) {
  BatchOperation(PendingOperation::COOKIE_UPDATEACCESS, cc);
}

void SQLitePersistentCookieStore::Backend::DeleteCookie(
    const CanonicalCookie& cc) {
  BatchOperation(PendingOperation::COOKIE_DELETE, cc);
}

void SQLitePersistentCookieStore::Backend::BatchOperation(
    PendingOperation::OperationType op,
    const CanonicalCookie& cc) {
  // Commit every 30 seconds.
  static const int kCommitIntervalMs = 30 * 1000;
  // Commit right away if we have more than 512 outstanding operations.
  static const size_t kCommitAfterBatchSize = 512;
  DCHECK(!background_task_runner()->RunsTasksInCurrentSequence());

  // We do a full copy of the cookie here, and hopefully just here.
  std::unique_ptr<PendingOperation> po(new PendingOperation(op, cc));

  PendingOperationsMap::size_type num_pending;
  {
    base::AutoLock locked(lock_);
    // When queueing the operation, see if it overwrites any already pending
    // ones for the same row.
    auto key = cc.UniqueKey();
    auto iter_and_result =
        pending_.insert(std::make_pair(key, PendingOperationsForKey()));
    PendingOperationsForKey& ops_for_key = iter_and_result.first->second;
    if (!iter_and_result.second) {
      // Insert failed -> already have ops.
      if (po->op() == PendingOperation::COOKIE_DELETE) {
        // A delete op makes all the previous ones irrelevant.
        ops_for_key.clear();
      } else if (po->op() == PendingOperation::COOKIE_UPDATEACCESS) {
        if (!ops_for_key.empty() &&
            ops_for_key.back()->op() == PendingOperation::COOKIE_UPDATEACCESS) {
          // If access timestamp is updated twice in a row, can dump the earlier
          // one.
          ops_for_key.pop_back();
        }
        // At most delete + add before (and no access time updates after above
        // conditional).
        DCHECK_LE(ops_for_key.size(), 2u);
      } else {
        // Nothing special is done for adds, since if they're overwriting,
        // they'll be preceded by deletes anyway.
        DCHECK_LE(ops_for_key.size(), 1u);
      }
    }
    ops_for_key.push_back(std::move(po));
    // Note that num_pending_ counts number of calls to BatchOperation(), not
    // the current length of the queue; this is intentional to guarantee
    // progress, as the length of the queue may decrease in some cases.
    num_pending = ++num_pending_;
  }

  if (num_pending == 1) {
    // We've gotten our first entry for this batch, fire off the timer.
    if (!background_task_runner()->PostDelayedTask(
            FROM_HERE, base::BindOnce(&Backend::Commit, this),
            base::TimeDelta::FromMilliseconds(kCommitIntervalMs))) {
      NOTREACHED() << "background_task_runner() is not running.";
    }
  } else if (num_pending == kCommitAfterBatchSize) {
    // We've reached a big enough batch, fire off a commit now.
    PostBackgroundTask(FROM_HERE, base::BindOnce(&Backend::Commit, this));
  }
}

void SQLitePersistentCookieStore::Backend::DoCommit() {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());

  PendingOperationsMap ops;
  {
    base::AutoLock locked(lock_);
    pending_.swap(ops);
    num_pending_ = 0;
  }

  // Maybe an old timer fired or we are already Close()'ed.
  if (!db() || ops.empty())
    return;

  sql::Statement add_statement(db()->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO cookies (creation_utc, top_frame_site_key, host_key, name, "
      "value, encrypted_value, path, expires_utc, is_secure, is_httponly, "
      "last_access_utc, has_expires, is_persistent, priority, samesite, "
      "source_scheme, source_port, is_same_party) "
      "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
  if (!add_statement.is_valid())
    return;

  sql::Statement update_access_statement(
      db()->GetCachedStatement(SQL_FROM_HERE,
                               "UPDATE cookies SET last_access_utc=? WHERE "
                               "name=? AND top_frame_site_key=? AND host_key=? "
                               "AND path=?"));
  if (!update_access_statement.is_valid())
    return;

  sql::Statement delete_statement(
      db()->GetCachedStatement(SQL_FROM_HERE,
                               "DELETE FROM cookies WHERE "
                               "name=? AND top_frame_site_key=? AND host_key=? "
                               "AND path=?"));
  if (!delete_statement.is_valid())
    return;

  sql::Transaction transaction(db());
  if (!transaction.Begin())
    return;

  for (auto& kv : ops) {
    for (std::unique_ptr<PendingOperation>& po_entry : kv.second) {
      // Free the cookies as we commit them to the database.
      std::unique_ptr<PendingOperation> po(std::move(po_entry));
      switch (po->op()) {
        case PendingOperation::COOKIE_ADD:
          add_statement.Reset(true);
          add_statement.BindTime(0, po->cc().CreationDate());
          // TODO(crbug.com/1225444) Serialize cookie's partition key into this
          // column.
          add_statement.BindString(1, net::kEmptyCookiePartitionKey);
          add_statement.BindString(2, po->cc().Domain());
          add_statement.BindString(3, po->cc().Name());
          if (crypto_ && crypto_->ShouldEncrypt()) {
            std::string encrypted_value;
            if (!crypto_->EncryptString(po->cc().Value(), &encrypted_value)) {
              DLOG(WARNING) << "Could not encrypt a cookie, skipping add.";
              RecordCookieCommitProblem(COOKIE_COMMIT_PROBLEM_ENCRYPT_FAILED);
              continue;
            }
            add_statement.BindCString(4, "");  // value
            // BindBlob() immediately makes an internal copy of the data.
            add_statement.BindBlob(5, encrypted_value);
          } else {
            add_statement.BindString(4, po->cc().Value());
            add_statement.BindBlob(5,
                                   base::span<uint8_t>());  // encrypted_value
          }
          add_statement.BindString(6, po->cc().Path());
          add_statement.BindTime(7, po->cc().ExpiryDate());
          add_statement.BindBool(8, po->cc().IsSecure());
          add_statement.BindBool(9, po->cc().IsHttpOnly());
          add_statement.BindTime(10, po->cc().LastAccessDate());
          add_statement.BindBool(11, po->cc().IsPersistent());
          add_statement.BindBool(12, po->cc().IsPersistent());
          add_statement.BindInt(
              13, CookiePriorityToDBCookiePriority(po->cc().Priority()));
          add_statement.BindInt(
              14, CookieSameSiteToDBCookieSameSite(po->cc().SameSite()));
          add_statement.BindInt(15, static_cast<int>(po->cc().SourceScheme()));
          add_statement.BindInt(16, po->cc().SourcePort());
          add_statement.BindBool(17, po->cc().IsSameParty());
          if (!add_statement.Run()) {
            DLOG(WARNING) << "Could not add a cookie to the DB.";
            RecordCookieCommitProblem(COOKIE_COMMIT_PROBLEM_ADD);
          }
          break;

        case PendingOperation::COOKIE_UPDATEACCESS:
          update_access_statement.Reset(true);
          update_access_statement.BindTime(0, po->cc().LastAccessDate());
          update_access_statement.BindString(1, po->cc().Name());
          // TODO(crbug.com/1225444) Update to use the cookie's actual
          // partition key.
          update_access_statement.BindString(2, net::kEmptyCookiePartitionKey);
          update_access_statement.BindString(3, po->cc().Domain());
          update_access_statement.BindString(4, po->cc().Path());
          if (!update_access_statement.Run()) {
            DLOG(WARNING)
                << "Could not update cookie last access time in the DB.";
            RecordCookieCommitProblem(COOKIE_COMMIT_PROBLEM_UPDATE_ACCESS);
          }
          break;

        case PendingOperation::COOKIE_DELETE:
          delete_statement.Reset(true);
          delete_statement.BindString(0, po->cc().Name());
          // TODO(crbug.com/1225444) Update to use the cookie's actual
          // partition key.
          delete_statement.BindString(1, net::kEmptyCookiePartitionKey);
          delete_statement.BindString(2, po->cc().Domain());
          delete_statement.BindString(3, po->cc().Path());
          if (!delete_statement.Run()) {
            DLOG(WARNING) << "Could not delete a cookie from the DB.";
            RecordCookieCommitProblem(COOKIE_COMMIT_PROBLEM_DELETE);
          }
          break;

        default:
          NOTREACHED();
          break;
      }
    }
  }
  bool commit_ok = transaction.Commit();
  if (!commit_ok) {
    RecordCookieCommitProblem(COOKIE_COMMIT_PROBLEM_TRANSACTION_COMMIT);
  }
}

size_t SQLitePersistentCookieStore::Backend::GetQueueLengthForTesting() {
  DCHECK(client_task_runner()->RunsTasksInCurrentSequence());
  size_t total = 0u;
  {
    base::AutoLock locked(lock_);
    for (const auto& key_val : pending_) {
      total += key_val.second.size();
    }
  }
  return total;
}

void SQLitePersistentCookieStore::Backend::DeleteAllInList(
    const std::list<CookieOrigin>& cookies) {
  if (cookies.empty())
    return;

  if (background_task_runner()->RunsTasksInCurrentSequence()) {
    BackgroundDeleteAllInList(cookies);
  } else {
    // Perform deletion on background task runner.
    PostBackgroundTask(
        FROM_HERE,
        base::BindOnce(&Backend::BackgroundDeleteAllInList, this, cookies));
  }
}

void SQLitePersistentCookieStore::Backend::DeleteSessionCookiesOnStartup() {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  if (!db()->Execute("DELETE FROM cookies WHERE is_persistent != 1"))
    LOG(WARNING) << "Unable to delete session cookies.";
}

// TODO(crbug.com/1225444) Investigate including top_frame_site_key in the WHERE
// clause.
void SQLitePersistentCookieStore::Backend::BackgroundDeleteAllInList(
    const std::list<CookieOrigin>& cookies) {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());

  if (!db())
    return;

  // Force a commit of any pending writes before issuing deletes.
  // TODO(rohitrao): Remove the need for this Commit() by instead pruning the
  // list of pending operations. https://crbug.com/486742.
  Commit();

  sql::Statement delete_statement(db()->GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM cookies WHERE host_key=? AND is_secure=?"));
  if (!delete_statement.is_valid()) {
    LOG(WARNING) << "Unable to delete cookies on shutdown.";
    return;
  }

  sql::Transaction transaction(db());
  if (!transaction.Begin()) {
    LOG(WARNING) << "Unable to delete cookies on shutdown.";
    return;
  }

  for (const auto& cookie : cookies) {
    const GURL url(cookie_util::CookieOriginToURL(cookie.first, cookie.second));
    if (!url.is_valid())
      continue;

    delete_statement.Reset(true);
    delete_statement.BindString(0, cookie.first);
    delete_statement.BindInt(1, cookie.second);
    if (!delete_statement.Run())
      NOTREACHED() << "Could not delete a cookie from the DB.";
  }

  if (!transaction.Commit())
    LOG(WARNING) << "Unable to delete cookies on shutdown.";
}

void SQLitePersistentCookieStore::Backend::FinishedLoadingCookies(
    LoadedCallback loaded_callback,
    bool success) {
  PostClientTask(FROM_HERE,
                 base::BindOnce(&Backend::CompleteLoadInForeground, this,
                                std::move(loaded_callback), success));
}

SQLitePersistentCookieStore::SQLitePersistentCookieStore(
    const base::FilePath& path,
    const scoped_refptr<base::SequencedTaskRunner>& client_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
    bool restore_old_session_cookies,
    CookieCryptoDelegate* crypto_delegate)
    : backend_(new Backend(path,
                           client_task_runner,
                           background_task_runner,
                           restore_old_session_cookies,
                           crypto_delegate)) {
}

void SQLitePersistentCookieStore::DeleteAllInList(
    const std::list<CookieOrigin>& cookies) {
  backend_->DeleteAllInList(cookies);
}

void SQLitePersistentCookieStore::Load(LoadedCallback loaded_callback,
                                       const NetLogWithSource& net_log) {
  DCHECK(!loaded_callback.is_null());
  net_log_ = net_log;
  net_log_.BeginEvent(NetLogEventType::COOKIE_PERSISTENT_STORE_LOAD);
  // Note that |backend_| keeps |this| alive by keeping a reference count.
  // If this class is ever converted over to a WeakPtr<> pattern (as TODO it
  // should be) this will need to be replaced by a more complex pattern that
  // guarantees |loaded_callback| being called even if the class has been
  // destroyed. |backend_| needs to outlive |this| to commit changes to disk.
  backend_->Load(base::BindOnce(&SQLitePersistentCookieStore::CompleteLoad,
                                this, std::move(loaded_callback)));
}

void SQLitePersistentCookieStore::LoadCookiesForKey(
    const std::string& key,
    LoadedCallback loaded_callback) {
  DCHECK(!loaded_callback.is_null());
  net_log_.AddEvent(NetLogEventType::COOKIE_PERSISTENT_STORE_KEY_LOAD_STARTED,
                    [&](NetLogCaptureMode capture_mode) {
                      return CookieKeyedLoadNetLogParams(key, capture_mode);
                    });
  // Note that |backend_| keeps |this| alive by keeping a reference count.
  // If this class is ever converted over to a WeakPtr<> pattern (as TODO it
  // should be) this will need to be replaced by a more complex pattern that
  // guarantees |loaded_callback| being called even if the class has been
  // destroyed. |backend_| needs to outlive |this| to commit changes to disk.
  backend_->LoadCookiesForKey(
      key, base::BindOnce(&SQLitePersistentCookieStore::CompleteKeyedLoad, this,
                          key, std::move(loaded_callback)));
}

void SQLitePersistentCookieStore::AddCookie(const CanonicalCookie& cc) {
  backend_->AddCookie(cc);
}

void SQLitePersistentCookieStore::UpdateCookieAccessTime(
    const CanonicalCookie& cc) {
  backend_->UpdateCookieAccessTime(cc);
}

void SQLitePersistentCookieStore::DeleteCookie(const CanonicalCookie& cc) {
  backend_->DeleteCookie(cc);
}

void SQLitePersistentCookieStore::SetForceKeepSessionState() {
  // This store never discards session-only cookies, so this call has no effect.
}

void SQLitePersistentCookieStore::SetBeforeCommitCallback(
    base::RepeatingClosure callback) {
  backend_->SetBeforeCommitCallback(std::move(callback));
}

void SQLitePersistentCookieStore::Flush(base::OnceClosure callback) {
  backend_->Flush(std::move(callback));
}

size_t SQLitePersistentCookieStore::GetQueueLengthForTesting() {
  return backend_->GetQueueLengthForTesting();
}

SQLitePersistentCookieStore::~SQLitePersistentCookieStore() {
  net_log_.AddEventWithStringParams(
      NetLogEventType::COOKIE_PERSISTENT_STORE_CLOSED, "type",
      "SQLitePersistentCookieStore");
  backend_->Close();
}

void SQLitePersistentCookieStore::CompleteLoad(
    LoadedCallback callback,
    std::vector<std::unique_ptr<CanonicalCookie>> cookie_list) {
  net_log_.EndEvent(NetLogEventType::COOKIE_PERSISTENT_STORE_LOAD);
  std::move(callback).Run(std::move(cookie_list));
}

void SQLitePersistentCookieStore::CompleteKeyedLoad(
    const std::string& key,
    LoadedCallback callback,
    std::vector<std::unique_ptr<CanonicalCookie>> cookie_list) {
  net_log_.AddEventWithStringParams(
      NetLogEventType::COOKIE_PERSISTENT_STORE_KEY_LOAD_COMPLETED, "domain",
      key);
  std::move(callback).Run(std::move(cookie_list));
}

}  // namespace net
