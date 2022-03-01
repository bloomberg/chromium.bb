// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/cert_db_initializer_impl.h"

#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "cert_db_initializer_io_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#include "chromeos/crosapi/mojom/cert_database.mojom.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/scoped_nss_types.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// This object is split between UI and IO threads:
// * CertDbInitializerImpl - is the outer layer that implements the KeyedService
// interface, implicitly tracks lifetime of its profile, triggers the
// initialization and implements the UI part of the NSSCertDatabaseGetter
// interface;
// * CertDbInitializerIOImpl - is the second (inner) part, it lives on the IO
// thread, manages lifetime of loaded NSS slots, NSSCertDatabase and implements
// the IO part of the NSSCertDatabaseGetter interface.
CertDbInitializerImpl::CertDbInitializerImpl(Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
  DCHECK(chromeos::LacrosService::Get()
             ->IsAvailable<crosapi::mojom::CertDatabase>());
  cert_db_initializer_io_ = std::make_unique<CertDbInitializerIOImpl>();
}

CertDbInitializerImpl::~CertDbInitializerImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // In case the initialization didn't finish, notify waiting observers.
  OnCertDbInitializationFinished();

  content::GetIOThreadTaskRunner({})->DeleteSoon(
      FROM_HERE, std::move(cert_db_initializer_io_));
}

void CertDbInitializerImpl::Start() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!profile_->IsMainProfile() || !chromeos::LacrosService::Get() ||
      !chromeos::LacrosService::Get()
           ->IsAvailable<crosapi::mojom::CertDatabase>()) {
    // TODO(b/191336028): Implement fully functional and separated certificate
    // database for secondary profiles when NSS library is replaced with
    // something more flexible.
    return InitializeReadOnlyCertDb();
  }

  // TODO(b/200784079): This is backwards compatibility code. It can be
  // removed in ChromeOS-M100.
  if (chromeos::LacrosService::Get()->GetInterfaceVersion(
          crosapi::mojom::CertDatabase::Uuid_) == 0) {
    return LegacyInitializeForMainProfile();
  }

  InitializeForMainProfile();
}

base::CallbackListSubscription CertDbInitializerImpl::WaitUntilReady(
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_ready_) {
    // Even if the database is ready, avoid calling the callback synchronously.
    // We still want to support returning a CallbackListSubscription, so this
    // code goes through callbacks_ in that case too, which will be notified in
    // OnCertDbInitializationFinished.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&CertDbInitializerImpl::OnCertDbInitializationFinished,
                       weak_factory_.GetWeakPtr()));
  }

  return callbacks_.Add(std::move(callback));
}

void CertDbInitializerImpl::InitializeReadOnlyCertDb() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto init_database_callback = base::BindPostTask(
      base::SequencedTaskRunnerHandle::Get(),
      base::BindOnce(&CertDbInitializerImpl::OnCertDbInitializationFinished,
                     weak_factory_.GetWeakPtr()));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CertDbInitializerIOImpl::InitializeReadOnlyNssCertDatabase,
          base::Unretained(cert_db_initializer_io_.get()),
          std::move(init_database_callback)));
}

void CertDbInitializerImpl::InitializeForMainProfile() {
  auto software_db_loaded_callback = base::BindPostTask(
      base::SequencedTaskRunnerHandle::Get(),
      base::BindOnce(&CertDbInitializerImpl::DidLoadSoftwareNssDb,
                     weak_factory_.GetWeakPtr()));

  const crosapi::mojom::BrowserInitParams* init_params =
      chromeos::LacrosService::Get()->init_params();
  base::FilePath nss_db_path;
  if (init_params->default_paths->user_nss_database) {
    nss_db_path = init_params->default_paths->user_nss_database.value();
  }

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&CertDbInitializerIOImpl::LoadSoftwareNssDb,
                                base::Unretained(cert_db_initializer_io_.get()),
                                std::move(nss_db_path),
                                std::move(software_db_loaded_callback)));
}

void CertDbInitializerImpl::DidLoadSoftwareNssDb() {
  chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::CertDatabase>()
      ->GetCertDatabaseInfo(
          base::BindOnce(&CertDbInitializerImpl::OnCertDbInfoReceived,
                         weak_factory_.GetWeakPtr()));
}

void CertDbInitializerImpl::OnCertDbInfoReceived(
    crosapi::mojom::GetCertDatabaseInfoResultPtr cert_db_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto init_database_callback = base::BindPostTask(
      base::SequencedTaskRunnerHandle::Get(),
      base::BindOnce(&CertDbInitializerImpl::OnCertDbInitializationFinished,
                     weak_factory_.GetWeakPtr()));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&CertDbInitializerIOImpl::InitializeNssCertDatabase,
                     base::Unretained(cert_db_initializer_io_.get()),
                     std::move(cert_db_info),
                     std::move(init_database_callback)));
}

void CertDbInitializerImpl::OnCertDbInitializationFinished() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  is_ready_ = true;
  callbacks_.Notify();
}

NssCertDatabaseGetter
CertDbInitializerImpl::CreateNssCertDatabaseGetterForIOThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return base::BindOnce(&CertDbInitializerIOImpl::GetNssCertDatabase,
                        base::Unretained(cert_db_initializer_io_.get()));
}

// ======================= Backwards compatibility code ========================

// TODO(b/200784079): This is backwards compatibility code. It can be
// removed in ChromeOS-M100.
void CertDbInitializerImpl::LegacyInitializeForMainProfile() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::CertDatabase>()
      ->GetCertDatabaseInfo(
          base::BindOnce(&CertDbInitializerImpl::OnLegacyCertDbInfoReceived,
                         weak_factory_.GetWeakPtr()));
}

// TODO(b/200784079): This is backwards compatibility code. It can be
// removed in ChromeOS-M100.
void CertDbInitializerImpl::OnLegacyCertDbInfoReceived(
    crosapi::mojom::GetCertDatabaseInfoResultPtr cert_db_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto init_database_callback = base::BindPostTask(
      base::SequencedTaskRunnerHandle::Get(),
      base::BindOnce(&CertDbInitializerImpl::OnCertDbInitializationFinished,
                     weak_factory_.GetWeakPtr()));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&CertDbInitializerIOImpl::InitializeLegacyNssCertDatabase,
                     base::Unretained(cert_db_initializer_io_.get()),
                     std::move(cert_db_info),
                     std::move(init_database_callback)));
}
