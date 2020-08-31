// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/web_database_host_impl.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/origin_util.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "storage/browser/database/database_util.h"
#include "storage/browser/database/vfs_backend.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/common/database/database_identifier.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "third_party/sqlite/sqlite3.h"
#include "url/origin.h"

using storage::DatabaseUtil;
using storage::VfsBackend;
using storage::QuotaManager;

namespace content {
namespace {
// The number of times to attempt to delete the SQLite database, if there is
// an error.
const int kNumDeleteRetries = 2;
// The delay between each retry to delete the SQLite database.
const int kDelayDeleteRetryMs = 100;

void ValidateOriginOnUIThread(
    int process_id,
    const url::Origin& origin,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceClosure success_callback,
    mojo::ReportBadMessageCallback error_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Return early if the process was shutdown before this task was able to run.
  if (!RenderProcessHost::FromID(process_id))
    return;

  if (!ChildProcessSecurityPolicyImpl::GetInstance()->CanAccessDataForOrigin(
          process_id, origin)) {
    callback_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback), "Unauthorized origin."));
    return;
  }

  callback_task_runner->PostTask(FROM_HERE, std::move(success_callback));
}

}  // namespace

WebDatabaseHostImpl::WebDatabaseHostImpl(
    int process_id,
    scoped_refptr<storage::DatabaseTracker> db_tracker)
    : process_id_(process_id),
      observer_added_(false),
      db_tracker_(std::move(db_tracker)) {
  DCHECK(db_tracker_);
}

WebDatabaseHostImpl::~WebDatabaseHostImpl() {
  DCHECK(db_tracker_->task_runner()->RunsTasksInCurrentSequence());

  if (observer_added_) {
    db_tracker_->RemoveObserver(this);

    // If the renderer process died without closing all databases,
    // then we need to manually close those connections
    db_tracker_->CloseDatabases(database_connections_);
    database_connections_.RemoveAllConnections();
  }
}

void WebDatabaseHostImpl::Create(
    int process_id,
    scoped_refptr<storage::DatabaseTracker> db_tracker,
    mojo::PendingReceiver<blink::mojom::WebDatabaseHost> receiver) {
  DCHECK(db_tracker->task_runner()->RunsTasksInCurrentSequence());
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<WebDatabaseHostImpl>(process_id, std::move(db_tracker)),
      std::move(receiver));
}

void WebDatabaseHostImpl::OpenFile(const base::string16& vfs_file_name,
                                   int32_t desired_flags,
                                   OpenFileCallback callback) {
  DCHECK(db_tracker_->task_runner()->RunsTasksInCurrentSequence());
  ValidateOrigin(vfs_file_name,
                 base::BindOnce(&WebDatabaseHostImpl::OpenFileValidated,
                                weak_ptr_factory_.GetWeakPtr(), vfs_file_name,
                                desired_flags, std::move(callback)));
}

void WebDatabaseHostImpl::OpenFileValidated(const base::string16& vfs_file_name,
                                            int32_t desired_flags,
                                            OpenFileCallback callback) {
  DCHECK(db_tracker_->task_runner()->RunsTasksInCurrentSequence());

  base::File file;
  const base::File* tracked_file = nullptr;
  std::string origin_identifier;
  base::string16 database_name;

  // When in OffTheRecord mode, we want to make sure that all DB files are
  // removed when the OffTheRecord browser context goes away, so we add the
  // SQLITE_OPEN_DELETEONCLOSE flag when opening all files, and keep
  // open handles to them in the database tracker to make sure they're
  // around for as long as needed.
  if (vfs_file_name.empty()) {
    file = VfsBackend::OpenTempFileInDirectory(
        db_tracker_->database_directory(), desired_flags);
  } else if (DatabaseUtil::CrackVfsFileName(vfs_file_name, &origin_identifier,
                                            &database_name, nullptr) &&
             !db_tracker_->IsDatabaseScheduledForDeletion(origin_identifier,
                                                          database_name)) {
    base::FilePath db_file = DatabaseUtil::GetFullFilePathForVfsFile(
        db_tracker_.get(), vfs_file_name);
    if (!db_file.empty()) {
      if (db_tracker_->IsOffTheRecordProfile()) {
        tracked_file = db_tracker_->GetOffTheRecordFile(vfs_file_name);
        if (!tracked_file) {
          file = VfsBackend::OpenFile(
              db_file, desired_flags | SQLITE_OPEN_DELETEONCLOSE);
          if (!(desired_flags & SQLITE_OPEN_DELETEONCLOSE)) {
            tracked_file = db_tracker_->SaveOffTheRecordFile(vfs_file_name,
                                                             std::move(file));
          }
        }
      } else {
        file = VfsBackend::OpenFile(db_file, desired_flags);
      }
    }
  }

  base::File result;
  if (file.IsValid()) {
    result = std::move(file);
  } else if (tracked_file) {
    DCHECK(tracked_file->IsValid());
    result = tracked_file->Duplicate();
  }
  std::move(callback).Run(std::move(result));
}

void WebDatabaseHostImpl::DeleteFile(const base::string16& vfs_file_name,
                                     bool sync_dir,
                                     DeleteFileCallback callback) {
  DCHECK(db_tracker_->task_runner()->RunsTasksInCurrentSequence());
  ValidateOrigin(
      vfs_file_name,
      base::BindOnce(&WebDatabaseHostImpl::DatabaseDeleteFile,
                     weak_ptr_factory_.GetWeakPtr(), vfs_file_name, sync_dir,
                     std::move(callback), kNumDeleteRetries));
}

void WebDatabaseHostImpl::GetFileAttributes(
    const base::string16& vfs_file_name,
    GetFileAttributesCallback callback) {
  DCHECK(db_tracker_->task_runner()->RunsTasksInCurrentSequence());
  ValidateOrigin(
      vfs_file_name,
      base::BindOnce(&WebDatabaseHostImpl::GetFileAttributesValidated,
                     weak_ptr_factory_.GetWeakPtr(), vfs_file_name,
                     std::move(callback)));
}

void WebDatabaseHostImpl::GetFileAttributesValidated(
    const base::string16& vfs_file_name,
    GetFileAttributesCallback callback) {
  DCHECK(db_tracker_->task_runner()->RunsTasksInCurrentSequence());

  int32_t attributes = -1;
  base::FilePath db_file =
      DatabaseUtil::GetFullFilePathForVfsFile(db_tracker_.get(), vfs_file_name);
  if (!db_file.empty()) {
    attributes = VfsBackend::GetFileAttributes(db_file);
  }
  std::move(callback).Run(attributes);
}

void WebDatabaseHostImpl::GetFileSize(const base::string16& vfs_file_name,
                                      GetFileSizeCallback callback) {
  DCHECK(db_tracker_->task_runner()->RunsTasksInCurrentSequence());
  ValidateOrigin(vfs_file_name,
                 base::BindOnce(&WebDatabaseHostImpl::GetFileSizeValidated,
                                weak_ptr_factory_.GetWeakPtr(), vfs_file_name,
                                std::move(callback)));
}

void WebDatabaseHostImpl::GetFileSizeValidated(
    const base::string16& vfs_file_name,
    GetFileSizeCallback callback) {
  DCHECK(db_tracker_->task_runner()->RunsTasksInCurrentSequence());

  int64_t size = 0LL;
  base::FilePath db_file =
      DatabaseUtil::GetFullFilePathForVfsFile(db_tracker_.get(), vfs_file_name);
  if (!db_file.empty()) {
    size = VfsBackend::GetFileSize(db_file);
  }
  std::move(callback).Run(size);
}

void WebDatabaseHostImpl::SetFileSize(const base::string16& vfs_file_name,
                                      int64_t expected_size,
                                      SetFileSizeCallback callback) {
  DCHECK(db_tracker_->task_runner()->RunsTasksInCurrentSequence());
  ValidateOrigin(vfs_file_name,
                 base::BindOnce(&WebDatabaseHostImpl::SetFileSizeValidated,
                                weak_ptr_factory_.GetWeakPtr(), vfs_file_name,
                                expected_size, std::move(callback)));
}

void WebDatabaseHostImpl::SetFileSizeValidated(
    const base::string16& vfs_file_name,
    int64_t expected_size,
    SetFileSizeCallback callback) {
  DCHECK(db_tracker_->task_runner()->RunsTasksInCurrentSequence());
  bool success = false;
  base::FilePath db_file =
      DatabaseUtil::GetFullFilePathForVfsFile(db_tracker_.get(), vfs_file_name);
  if (!db_file.empty()) {
    success = VfsBackend::SetFileSize(db_file, expected_size);
  }
  std::move(callback).Run(success);
}

void WebDatabaseHostImpl::GetSpaceAvailable(
    const url::Origin& origin,
    GetSpaceAvailableCallback callback) {
  // QuotaManager is only available on the IO thread.
  DCHECK(db_tracker_->task_runner()->RunsTasksInCurrentSequence());

  ValidateOrigin(
      origin, base::BindOnce(&WebDatabaseHostImpl::GetSpaceAvailableValidated,
                             weak_ptr_factory_.GetWeakPtr(), origin,
                             std::move(callback)));
}

void WebDatabaseHostImpl::GetSpaceAvailableValidated(
    const url::Origin& origin,
    GetSpaceAvailableCallback callback) {
  // QuotaManager is only available on the IO thread.
  DCHECK(db_tracker_->task_runner()->RunsTasksInCurrentSequence());

  DCHECK(db_tracker_->quota_manager_proxy());
  db_tracker_->quota_manager_proxy()->GetUsageAndQuota(
      db_tracker_->task_runner(), origin, blink::mojom::StorageType::kTemporary,
      base::BindOnce(
          [](GetSpaceAvailableCallback callback,
             blink::mojom::QuotaStatusCode status, int64_t usage,
             int64_t quota) {
            int64_t available = 0;
            if ((status == blink::mojom::QuotaStatusCode::kOk) &&
                (usage < quota)) {
              available = quota - usage;
            }
            std::move(callback).Run(available);
          },
          std::move(callback)));
}

void WebDatabaseHostImpl::DatabaseDeleteFile(
    const base::string16& vfs_file_name,
    bool sync_dir,
    DeleteFileCallback callback,
    int reschedule_count) {
  DCHECK(db_tracker_->task_runner()->RunsTasksInCurrentSequence());

  // Return an error if the file name is invalid or if the file could not
  // be deleted after kNumDeleteRetries attempts.
  int error_code = SQLITE_IOERR_DELETE;
  base::FilePath db_file =
      DatabaseUtil::GetFullFilePathForVfsFile(db_tracker_.get(), vfs_file_name);
  if (!db_file.empty()) {
    // In order to delete a journal file in OffTheRecord mode, we only need to
    // close the open handle to it that's stored in the database tracker.
    if (db_tracker_->IsOffTheRecordProfile()) {
      const base::string16 wal_suffix(base::ASCIIToUTF16("-wal"));
      base::string16 sqlite_suffix;

      // WAL files can be deleted without having previously been opened.
      if (!db_tracker_->HasSavedOffTheRecordFileHandle(vfs_file_name) &&
          DatabaseUtil::CrackVfsFileName(vfs_file_name, nullptr, nullptr,
                                         &sqlite_suffix) &&
          sqlite_suffix == wal_suffix) {
        error_code = SQLITE_OK;
      } else {
        db_tracker_->CloseOffTheRecordFileHandle(vfs_file_name);
        error_code = SQLITE_OK;
      }
    } else {
      error_code = VfsBackend::DeleteFile(db_file, sync_dir);
    }

    if ((error_code == SQLITE_IOERR_DELETE) && reschedule_count) {
      // If the file could not be deleted, try again.
      db_tracker_->task_runner()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&WebDatabaseHostImpl::DatabaseDeleteFile,
                         weak_ptr_factory_.GetWeakPtr(), vfs_file_name,
                         sync_dir, std::move(callback), reschedule_count - 1),
          base::TimeDelta::FromMilliseconds(kDelayDeleteRetryMs));
      return;
    }
  }

  std::move(callback).Run(error_code);
}

void WebDatabaseHostImpl::Opened(const url::Origin& origin,
                                 const base::string16& database_name,
                                 const base::string16& database_description,
                                 int64_t estimated_size) {
  DCHECK(db_tracker_->task_runner()->RunsTasksInCurrentSequence());

  if (!observer_added_) {
    observer_added_ = true;
    db_tracker_->AddObserver(this);
  }

  ValidateOrigin(origin, base::BindOnce(&WebDatabaseHostImpl::OpenedValidated,
                                        weak_ptr_factory_.GetWeakPtr(), origin,
                                        database_name, database_description,
                                        estimated_size));
}

void WebDatabaseHostImpl::OpenedValidated(
    const url::Origin& origin,
    const base::string16& database_name,
    const base::string16& database_description,
    int64_t estimated_size) {
  DCHECK(db_tracker_->task_runner()->RunsTasksInCurrentSequence());

  UMA_HISTOGRAM_BOOLEAN("websql.OpenDatabase", IsOriginSecure(origin.GetURL()));

  int64_t database_size = 0;
  std::string origin_identifier(storage::GetIdentifierFromOrigin(origin));
  db_tracker_->DatabaseOpened(origin_identifier, database_name,
                              database_description, estimated_size,
                              &database_size);

  database_connections_.AddConnection(origin_identifier, database_name);

  GetWebDatabase().UpdateSize(origin, database_name, database_size);
}

void WebDatabaseHostImpl::Modified(const url::Origin& origin,
                                   const base::string16& database_name) {
  DCHECK(db_tracker_->task_runner()->RunsTasksInCurrentSequence());

  ValidateOrigin(origin, base::BindOnce(&WebDatabaseHostImpl::ModifiedValidated,
                                        weak_ptr_factory_.GetWeakPtr(), origin,
                                        database_name));
}

void WebDatabaseHostImpl::ModifiedValidated(
    const url::Origin& origin,
    const base::string16& database_name) {
  DCHECK(db_tracker_->task_runner()->RunsTasksInCurrentSequence());
  std::string origin_identifier(storage::GetIdentifierFromOrigin(origin));
  if (!database_connections_.IsDatabaseOpened(origin_identifier,
                                              database_name)) {
    mojo::ReportBadMessage("Database not opened on modify");
    return;
  }

  db_tracker_->DatabaseModified(origin_identifier, database_name);
}

void WebDatabaseHostImpl::Closed(const url::Origin& origin,
                                 const base::string16& database_name) {
  DCHECK(db_tracker_->task_runner()->RunsTasksInCurrentSequence());

  ValidateOrigin(origin, base::BindOnce(&WebDatabaseHostImpl::ClosedValidated,
                                        weak_ptr_factory_.GetWeakPtr(), origin,
                                        database_name));
}

void WebDatabaseHostImpl::ClosedValidated(const url::Origin& origin,
                                          const base::string16& database_name) {
  DCHECK(db_tracker_->task_runner()->RunsTasksInCurrentSequence());

  std::string origin_identifier(storage::GetIdentifierFromOrigin(origin));
  if (!database_connections_.IsDatabaseOpened(origin_identifier,
                                              database_name)) {
    mojo::ReportBadMessage("Database not opened on close");
    return;
  }

  database_connections_.RemoveConnection(origin_identifier, database_name);
  db_tracker_->DatabaseClosed(origin_identifier, database_name);
}

void WebDatabaseHostImpl::HandleSqliteError(const url::Origin& origin,
                                            const base::string16& database_name,
                                            int32_t error) {
  DCHECK(db_tracker_->task_runner()->RunsTasksInCurrentSequence());
  ValidateOrigin(
      origin,
      base::BindOnce(&storage::DatabaseTracker::HandleSqliteError, db_tracker_,
                     storage::GetIdentifierFromOrigin(origin), database_name,
                     error));
}

void WebDatabaseHostImpl::OnDatabaseSizeChanged(
    const std::string& origin_identifier,
    const base::string16& database_name,
    int64_t database_size) {
  DCHECK(db_tracker_->task_runner()->RunsTasksInCurrentSequence());
  if (!database_connections_.IsOriginUsed(origin_identifier)) {
    return;
  }

  GetWebDatabase().UpdateSize(
      storage::GetOriginFromIdentifier(origin_identifier), database_name,
      database_size);
}

void WebDatabaseHostImpl::OnDatabaseScheduledForDeletion(
    const std::string& origin_identifier,
    const base::string16& database_name) {
  DCHECK(db_tracker_->task_runner()->RunsTasksInCurrentSequence());

  GetWebDatabase().CloseImmediately(
      storage::GetOriginFromIdentifier(origin_identifier), database_name);
}

blink::mojom::WebDatabase& WebDatabaseHostImpl::GetWebDatabase() {
  DCHECK(db_tracker_->task_runner()->RunsTasksInCurrentSequence());
  if (!database_provider_) {
    // The interface binding needs to occur on the UI thread, as we can
    // only call RenderProcessHost::FromID() on the UI thread.
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(
            [](int process_id,
               mojo::PendingReceiver<blink::mojom::WebDatabase> receiver) {
              RenderProcessHost* host = RenderProcessHost::FromID(process_id);
              if (host)
                host->BindReceiver(std::move(receiver));
            },
            process_id_, database_provider_.BindNewPipeAndPassReceiver()));
  }
  return *database_provider_.get();
}

void WebDatabaseHostImpl::ValidateOrigin(const url::Origin& origin,
                                         base::OnceClosure callback) {
  if (origin.opaque()) {
    mojo::ReportBadMessage("Invalid origin.");
    return;
  }

  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&ValidateOriginOnUIThread, process_id_, origin,
                     base::RetainedRef(db_tracker_->task_runner()),
                     std::move(callback), mojo::GetBadMessageCallback()));
}

void WebDatabaseHostImpl::ValidateOrigin(const base::string16& vfs_file_name,
                                         base::OnceClosure callback) {
  std::string origin_identifier;
  if (vfs_file_name.empty()) {
    std::move(callback).Run();
    return;
  }

  if (!DatabaseUtil::CrackVfsFileName(vfs_file_name, &origin_identifier,
                                      nullptr, nullptr)) {
    std::move(callback).Run();
    return;
  }

  ValidateOrigin(storage::GetOriginFromIdentifier(origin_identifier),
                 std::move(callback));
}

}  // namespace content
