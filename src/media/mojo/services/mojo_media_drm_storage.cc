// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_media_drm_storage.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

namespace media {

// TODO(xhwang): When connection error happens, callbacks might be dropped and
// never run. Handle connection error to make sure callbacks will always run.

MojoMediaDrmStorage::MojoMediaDrmStorage(
    mojom::MediaDrmStoragePtr media_drm_storage_ptr)
    : media_drm_storage_ptr_(std::move(media_drm_storage_ptr)),
      weak_factory_(this) {
  DVLOG(1) << __func__;
}

MojoMediaDrmStorage::~MojoMediaDrmStorage() {}

void MojoMediaDrmStorage::Initialize(InitCB init_cb) {
  DVLOG(1) << __func__;
  media_drm_storage_ptr_->Initialize(
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(init_cb),
                                                  base::UnguessableToken()));
}

void MojoMediaDrmStorage::OnProvisioned(ResultCB result_cb) {
  DVLOG(1) << __func__;
  media_drm_storage_ptr_->OnProvisioned(
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(result_cb), false));
}

void MojoMediaDrmStorage::SavePersistentSession(const std::string& session_id,
                                                const SessionData& session_data,
                                                ResultCB result_cb) {
  DVLOG(1) << __func__;
  media_drm_storage_ptr_->SavePersistentSession(
      session_id,
      mojom::SessionData::New(session_data.key_set_id, session_data.mime_type),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(result_cb), false));
}

void MojoMediaDrmStorage::LoadPersistentSession(
    const std::string& session_id,
    LoadPersistentSessionCB load_persistent_session_cb) {
  DVLOG(1) << __func__;
  media_drm_storage_ptr_->LoadPersistentSession(
      session_id,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&MojoMediaDrmStorage::OnPersistentSessionLoaded,
                         weak_factory_.GetWeakPtr(),
                         base::Passed(&load_persistent_session_cb)),
          nullptr));
}

void MojoMediaDrmStorage::RemovePersistentSession(const std::string& session_id,
                                                  ResultCB result_cb) {
  DVLOG(1) << __func__;
  media_drm_storage_ptr_->RemovePersistentSession(
      session_id,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(result_cb), false));
}

void MojoMediaDrmStorage::OnPersistentSessionLoaded(
    LoadPersistentSessionCB load_persistent_session_cb,
    mojom::SessionDataPtr session_data) {
  DVLOG(1) << __func__ << ": success = " << !!session_data;

  std::move(load_persistent_session_cb)
      .Run(session_data ? std::make_unique<SessionData>(
                              std::move(session_data->key_set_id),
                              std::move(session_data->mime_type))
                        : nullptr);
}

}  // namespace media
