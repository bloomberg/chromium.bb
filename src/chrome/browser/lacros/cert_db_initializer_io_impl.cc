// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/cert_db_initializer_io_impl.h"

#include "base/debug/dump_without_crashing.h"
#include "base/files/file_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "cert_db_initializer_io_impl.h"
#include "chromeos/crosapi/mojom/cert_database.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/chaps_support.h"
#include "crypto/nss_util.h"
#include "crypto/nss_util_internal.h"

namespace {

// Loads software NSS database a returns a slot referencing it (a.k.a. public
// slot). Should be called on a worked thread because it performs blocking
// operations. Crashes Chrome if it fails to load the database (mostly because
// it'd crash anyway in the CHECK(public_slot) in NSSCertDatabase).
crypto::ScopedPK11Slot LoadSoftwareNssDbOnWorkerThread(
    const base::FilePath software_nss_database_path) {
  crypto::EnsureNSSInit();

  if (software_nss_database_path.empty()) {
    CHECK(false);
    return {};
  }

  if (!base::CreateDirectory(software_nss_database_path)) {
    CHECK(false) << "Failed to create " << software_nss_database_path.value()
                 << " directory.";
    return {};
  }

  // `description` doesn't affect anything.
  crypto::ScopedPK11Slot public_slot =
      crypto::OpenSoftwareNSSDB(software_nss_database_path,
                                /*description=*/"cert_db");

  CHECK(public_slot);
  return public_slot;
}

void LoadSlotsOnWorkerThread(
    crosapi::mojom::GetCertDatabaseInfoResultPtr cert_db_info,
    base::OnceCallback<void(crypto::ScopedPK11Slot private_slot,
                            crypto::ScopedPK11Slot system_slot)> callback) {
  crypto::EnsureNSSInit();

  if (!cert_db_info || !cert_db_info->should_load_chaps) {
    // Lacros failed to retrieve initialization data from Ash or Ash explicitly
    // replied that chaps shouldn't be loaded.
    std::move(callback).Run(
        /*private_slot=*/crypto::ScopedPK11Slot(PK11_GetInternalKeySlot()),
        /*system_slot=*/crypto::ScopedPK11Slot());
    return;
  }

  SECMODModule* chaps = crypto::LoadChaps();
  if (!chaps) {
    // Chaps should never fail loading, but try to gracefully recover while
    // also logging a crashdump.
    base::debug::DumpWithoutCrashing();
    std::move(callback).Run(
        /*private_slot=*/crypto::ScopedPK11Slot(PK11_GetInternalKeySlot()),
        /*system_slot=*/crypto::ScopedPK11Slot());
    return;
  }

  crypto::ScopedPK11Slot private_slot =
      crypto::GetChapsSlot(chaps, cert_db_info->private_slot_id);
  if (!private_slot) {
    base::debug::DumpWithoutCrashing();
    std::move(callback).Run(
        /*private_slot=*/crypto::ScopedPK11Slot(PK11_GetInternalKeySlot()),
        /*system_slot=*/crypto::ScopedPK11Slot());
    return;
  }

  if (!cert_db_info->enable_system_slot) {
    std::move(callback).Run(std::move(private_slot),
                            /*system_slot=*/crypto::ScopedPK11Slot());
    return;
  }

  crypto::ScopedPK11Slot system_slot =
      crypto::GetChapsSlot(chaps, cert_db_info->system_slot_id);
  if (!system_slot) {
    base::debug::DumpWithoutCrashing();
    // Proceed with creating the normal database to at least give users
    // certificates from the private slot.
  }
  std::move(callback).Run(std::move(private_slot), std::move(system_slot));
}

// TODO(b/200784079): This is backwards compatibility code. It can be
// removed in ChromeOS-M100.
void LegacyInitializeCertDbOnWorkerThread(
    crosapi::mojom::GetCertDatabaseInfoResultPtr cert_db_info) {
  crypto::EnsureNSSInit();

  if (cert_db_info->should_load_chaps) {
    // NSS functions may reenter //net via extension hooks. If the reentered
    // code needs to synchronously wait for a task to run but the thread pool in
    // which that task must run doesn't have enough threads to schedule it, a
    // deadlock occurs. To prevent that, the base::ScopedBlockingCall below
    // increments the thread pool capacity for the duration of the TPM
    // initialization.
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::WILL_BLOCK);
    // There's no need to save the result of `LoadChaps()`, chaps will stay
    // loaded and can be used anyway. Using the result only to determine
    // success/failure.
    if (!crypto::LoadChaps()) {
      return;
    }
  }

  // The slot doesn't need to be saved either. `description` doesn't affect
  // anything. `software_nss_db_path` file path should be already created by
  // Ash-Chrome.
  auto slot = crypto::OpenSoftwareNSSDB(
      base::FilePath(cert_db_info->DEPRECATED_software_nss_db_path),
      /*description=*/"cert_db");
  if (!slot) {
    LOG(ERROR) << "Failed to open user certificate database";
  }
}

}  // namespace

//------------------------------------------------------------------------------

CertDbInitializerIOImpl::CertDbInitializerIOImpl() = default;

CertDbInitializerIOImpl::~CertDbInitializerIOImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

net::NSSCertDatabase* CertDbInitializerIOImpl::GetNssCertDatabase(
    GetNSSCertDatabaseCallback callback) {
  if (nss_cert_database_) {
    return nss_cert_database_.get();
  }

  ready_callback_list_.AddUnsafe(std::move(callback));
  return nullptr;
}

void CertDbInitializerIOImpl::LoadSoftwareNssDb(
    const base::FilePath& user_nss_database_path,
    base::OnceClosure load_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!pending_public_slot_);
  DCHECK(!nss_cert_database_);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&LoadSoftwareNssDbOnWorkerThread, user_nss_database_path),
      base::BindOnce(&CertDbInitializerIOImpl::DidLoadSoftwareNssDb,
                     weak_factory_.GetWeakPtr(), std::move(load_callback)));
}

void CertDbInitializerIOImpl::DidLoadSoftwareNssDb(
    base::OnceClosure load_callback,
    crypto::ScopedPK11Slot public_slot) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!pending_public_slot_);
  DCHECK(!nss_cert_database_);

  pending_public_slot_ = std::move(public_slot);
  std::move(load_callback).Run();
}

void CertDbInitializerIOImpl::InitializeNssCertDatabase(
    crosapi::mojom::GetCertDatabaseInfoResultPtr cert_db_info,
    base::OnceClosure init_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(pending_public_slot_) << "LoadSoftwareNssDb() must be called first.";
  DCHECK(!nss_cert_database_);

  auto load_slots_callback = base::BindPostTask(
      base::SequencedTaskRunnerHandle::Get(),
      base::BindOnce(&CertDbInitializerIOImpl::DidLoadSlots,
                     weak_factory_.GetWeakPtr(), std::move(init_callback)));

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&LoadSlotsOnWorkerThread, std::move(cert_db_info),
                     std::move(load_slots_callback)));
}

void CertDbInitializerIOImpl::DidLoadSlots(base::OnceClosure init_callback,
                                           crypto::ScopedPK11Slot private_slot,
                                           crypto::ScopedPK11Slot system_slot) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!nss_cert_database_);

  nss_cert_database_ = std::make_unique<net::NSSCertDatabaseChromeOS>(
      std::move(pending_public_slot_), std::move(private_slot));
  nss_cert_database_->SetSystemSlot(std::move(system_slot));

  std::move(init_callback).Run();
  ready_callback_list_.Notify(nss_cert_database_.get());
}

void CertDbInitializerIOImpl::InitializeReadOnlyNssCertDatabase(
    base::OnceClosure init_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!pending_public_slot_);
  DCHECK(!nss_cert_database_);

  crypto::EnsureNSSInit();
  nss_cert_database_ = std::make_unique<net::NSSCertDatabaseChromeOS>(
      /*public_slot=*/crypto::ScopedPK11Slot(PK11_GetInternalKeySlot()),
      /*private_slot=*/crypto::ScopedPK11Slot(PK11_GetInternalKeySlot()));
  std::move(init_callback).Run();
  ready_callback_list_.Notify(nss_cert_database_.get());
}

// TODO(b/200784079): This is backwards compatibility code. It can be
// removed in ChromeOS-M100.
void CertDbInitializerIOImpl::InitializeLegacyNssCertDatabase(
    crosapi::mojom::GetCertDatabaseInfoResultPtr cert_db_info,
    base::OnceClosure init_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!pending_public_slot_);
  DCHECK(!nss_cert_database_);

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&LegacyInitializeCertDbOnWorkerThread,
                     std::move(cert_db_info)),
      base::BindOnce(&CertDbInitializerIOImpl::DidLegacyInitialize,
                     weak_factory_.GetWeakPtr(), std::move(init_callback)));
}

// TODO(b/200784079): This is backwards compatibility code. It can be
// removed in ChromeOS-M100.
void CertDbInitializerIOImpl::DidLegacyInitialize(
    base::OnceClosure init_callback) {
  InitializeReadOnlyNssCertDatabase(std::move(init_callback));
}
