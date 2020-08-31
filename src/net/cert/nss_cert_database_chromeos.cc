// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/nss_cert_database_chromeos.h"

#include <cert.h>
#include <pk11pub.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"

namespace net {

NSSCertDatabaseChromeOS::NSSCertDatabaseChromeOS(
    crypto::ScopedPK11Slot public_slot,
    crypto::ScopedPK11Slot private_slot)
    : NSSCertDatabase(std::move(public_slot), std::move(private_slot)) {
  // By default, don't use a system slot. Only if explicitly set by
  // SetSystemSlot, the system slot will be used.
  profile_filter_.Init(GetPublicSlot(),
                       GetPrivateSlot(),
                       crypto::ScopedPK11Slot() /* no system slot */);
}

NSSCertDatabaseChromeOS::~NSSCertDatabaseChromeOS() = default;

void NSSCertDatabaseChromeOS::SetSystemSlot(
    crypto::ScopedPK11Slot system_slot) {
  system_slot_ = std::move(system_slot);
  profile_filter_.Init(GetPublicSlot(), GetPrivateSlot(), GetSystemSlot());
}

void NSSCertDatabaseChromeOS::ListCerts(
    NSSCertDatabase::ListCertsCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&NSSCertDatabaseChromeOS::ListCertsImpl, profile_filter_),
      std::move(callback));
}

void NSSCertDatabaseChromeOS::ListCertsInfo(ListCertsInfoCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&NSSCertDatabaseChromeOS::ListCertsInfoImpl,
                     profile_filter_, /*slot=*/GetSystemSlot(),
                     /*add_certs_info=*/true),
      std::move(callback));
}

crypto::ScopedPK11Slot NSSCertDatabaseChromeOS::GetSystemSlot() const {
  if (system_slot_)
    return crypto::ScopedPK11Slot(PK11_ReferenceSlot(system_slot_.get()));
  return crypto::ScopedPK11Slot();
}

void NSSCertDatabaseChromeOS::ListModules(
    std::vector<crypto::ScopedPK11Slot>* modules,
    bool need_rw) const {
  NSSCertDatabase::ListModules(modules, need_rw);

  size_t pre_size = modules->size();
  const NSSProfileFilterChromeOS& profile_filter = profile_filter_;
  base::EraseIf(*modules, [&profile_filter](crypto::ScopedPK11Slot& module) {
    return !profile_filter.IsModuleAllowed(module.get());
  });
  DVLOG(1) << "filtered " << pre_size - modules->size() << " of " << pre_size
           << " modules";
}

// static
ScopedCERTCertificateList NSSCertDatabaseChromeOS::ListCertsImpl(
    const NSSProfileFilterChromeOS& profile_filter) {
  CertInfoList certs_info = ListCertsInfoImpl(
      profile_filter, crypto::ScopedPK11Slot(), /*add_certs_info=*/false);

  return ExtractCertificates(std::move(certs_info));
}

// static
NSSCertDatabase::CertInfoList NSSCertDatabaseChromeOS::ListCertsInfoImpl(
    const NSSProfileFilterChromeOS& profile_filter,
    crypto::ScopedPK11Slot system_slot,
    bool add_certs_info) {
  // This method may acquire the NSS lock or reenter this code via extension
  // hooks (such as smart card UI). To ensure threads are not starved or
  // deadlocked, the base::ScopedBlockingCall below increments the thread pool
  // capacity if this method takes too much time to run.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  CertInfoList certs_info(NSSCertDatabase::ListCertsInfoImpl(
      crypto::ScopedPK11Slot(), add_certs_info));

  // Filter certificate information according to user profile.
  size_t pre_size = certs_info.size();
  base::EraseIf(certs_info, [&profile_filter](CertInfo& cert_info) {
    return !profile_filter.IsCertAllowed(cert_info.cert.get());
  });
  DVLOG(1) << "filtered " << pre_size - certs_info.size() << " of " << pre_size
           << " certs";

  if (add_certs_info) {
    // Add Chrome OS specific information.
    for (auto& cert_info : certs_info) {
      cert_info.device_wide =
          IsCertificateOnSlot(cert_info.cert.get(), system_slot.get());
      cert_info.hardware_backed = IsHardwareBacked(cert_info.cert.get());
    }
  }

  return certs_info;
}

}  // namespace net
