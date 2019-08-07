// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_service_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "content/browser/background_sync/background_sync_context_impl.h"
#include "content/public/browser/browser_thread.h"

namespace content {

#define COMPILE_ASSERT_MATCHING_ENUM(mojo_name, manager_name) \
  static_assert(static_cast<int>(blink::mojo_name) ==         \
                    static_cast<int>(content::manager_name),  \
                "mojo and manager enums must match")

// TODO(iclelland): Move these tests somewhere else
COMPILE_ASSERT_MATCHING_ENUM(mojom::BackgroundSyncError::NONE,
                             BACKGROUND_SYNC_STATUS_OK);
COMPILE_ASSERT_MATCHING_ENUM(mojom::BackgroundSyncError::STORAGE,
                             BACKGROUND_SYNC_STATUS_STORAGE_ERROR);
COMPILE_ASSERT_MATCHING_ENUM(mojom::BackgroundSyncError::NOT_FOUND,
                             BACKGROUND_SYNC_STATUS_NOT_FOUND);
COMPILE_ASSERT_MATCHING_ENUM(mojom::BackgroundSyncError::NO_SERVICE_WORKER,
                             BACKGROUND_SYNC_STATUS_NO_SERVICE_WORKER);
COMPILE_ASSERT_MATCHING_ENUM(mojom::BackgroundSyncError::NOT_ALLOWED,
                             BACKGROUND_SYNC_STATUS_NOT_ALLOWED);
COMPILE_ASSERT_MATCHING_ENUM(mojom::BackgroundSyncError::PERMISSION_DENIED,
                             BACKGROUND_SYNC_STATUS_PERMISSION_DENIED);
COMPILE_ASSERT_MATCHING_ENUM(mojom::BackgroundSyncError::MAX,
                             BACKGROUND_SYNC_STATUS_PERMISSION_DENIED);

BackgroundSyncServiceImpl::~BackgroundSyncServiceImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(background_sync_context_->background_sync_manager());
}

BackgroundSyncServiceImpl::BackgroundSyncServiceImpl(
    BackgroundSyncContextImpl* background_sync_context,
    mojo::InterfaceRequest<blink::mojom::BackgroundSyncService> request)
    : background_sync_context_(background_sync_context),
      binding_(this, std::move(request)),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(background_sync_context);

  binding_.set_connection_error_handler(base::BindOnce(
      &BackgroundSyncServiceImpl::OnConnectionError,
      base::Unretained(this) /* the channel is owned by this */));
}

void BackgroundSyncServiceImpl::OnConnectionError() {
  background_sync_context_->ServiceHadConnectionError(this);
  // |this| is now deleted.
}

void BackgroundSyncServiceImpl::Register(
    blink::mojom::SyncRegistrationOptionsPtr options,
    int64_t sw_registration_id,
    RegisterCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  BackgroundSyncManager* background_sync_manager =
      background_sync_context_->background_sync_manager();
  DCHECK(background_sync_manager);
  background_sync_manager->Register(
      sw_registration_id, *options,
      base::BindOnce(&BackgroundSyncServiceImpl::OnRegisterResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void BackgroundSyncServiceImpl::DidResolveRegistration(
    blink::mojom::BackgroundSyncRegistrationInfoPtr registration_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  BackgroundSyncManager* background_sync_manager =
      background_sync_context_->background_sync_manager();
  DCHECK(background_sync_manager);
  background_sync_manager->DidResolveRegistration(std::move(registration_info));
}

void BackgroundSyncServiceImpl::GetOneShotSyncRegistrations(
    int64_t sw_registration_id,
    GetOneShotSyncRegistrationsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BackgroundSyncManager* background_sync_manager =
      background_sync_context_->background_sync_manager();
  DCHECK(background_sync_manager);
  background_sync_manager->GetOneShotSyncRegistrations(
      sw_registration_id,
      base::BindOnce(&BackgroundSyncServiceImpl::OnGetRegistrationsResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void BackgroundSyncServiceImpl::OnRegisterResult(
    RegisterCallback callback,
    BackgroundSyncStatus status,
    std::unique_ptr<BackgroundSyncRegistration> result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (status != BACKGROUND_SYNC_STATUS_OK) {
    std::move(callback).Run(
        static_cast<blink::mojom::BackgroundSyncError>(status),
        blink::mojom::SyncRegistrationOptions::New());
    return;
  }

  DCHECK(result);
  blink::mojom::SyncRegistrationOptionsPtr mojo_options =
      blink::mojom::SyncRegistrationOptions::New(*result->options());
  std::move(callback).Run(
      static_cast<blink::mojom::BackgroundSyncError>(status),
      std::move(mojo_options));
}

void BackgroundSyncServiceImpl::OnGetRegistrationsResult(
    GetOneShotSyncRegistrationsCallback callback,
    BackgroundSyncStatus status,
    std::vector<std::unique_ptr<BackgroundSyncRegistration>>
        result_registrations) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::vector<blink::mojom::SyncRegistrationOptionsPtr> mojo_registrations;
  for (const auto& registration : result_registrations) {
    mojo_registrations.push_back(
        blink::mojom::SyncRegistrationOptions::New(*registration->options()));
  }

  std::move(callback).Run(
      static_cast<blink::mojom::BackgroundSyncError>(status),
      std::move(mojo_registrations));
}

}  // namespace content
