// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/login/auth/mount_performer.h"

#include "ash/components/cryptohome/userdataauth_util.h"
#include "ash/components/login/auth/user_context.h"
#include "base/bind.h"
#include "base/callback.h"
#include "chromeos/dbus/userdataauth/userdataauth_client.h"
#include "components/device_event_log/device_event_log.h"

namespace ash {

MountPerformer::MountPerformer() = default;
MountPerformer::~MountPerformer() = default;

void MountPerformer::InvalidateCurrentAttempts() {
  weak_factory_.InvalidateWeakPtrs();
}

void MountPerformer::CreateNewUser(std::unique_ptr<UserContext> context,
                                   AuthOperationCallback callback) {
  user_data_auth::CreatePersistentUserRequest request;
  LOGIN_LOG(EVENT) << "Create persistent directory";
  request.set_auth_session_id(context->GetAuthSessionId());
  UserDataAuthClient::Get()->CreatePersistentUser(
      request, base::BindOnce(&MountPerformer::OnCreatePersistentUser,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

void MountPerformer::MountPersistentDirectory(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  user_data_auth::PreparePersistentVaultRequest request;
  LOGIN_LOG(EVENT) << "Mount persistent directory";
  request.set_auth_session_id(context->GetAuthSessionId());
  request.set_block_ecryptfs(context->IsForcingDircrypto());
  UserDataAuthClient::Get()->PreparePersistentVault(
      request, base::BindOnce(&MountPerformer::OnPreparePersistentVault,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

void MountPerformer::MountForMigration(std::unique_ptr<UserContext> context,
                                       AuthOperationCallback callback) {
  user_data_auth::PrepareVaultForMigrationRequest request;
  request.set_auth_session_id(context->GetAuthSessionId());
  UserDataAuthClient::Get()->PrepareVaultForMigration(
      request, base::BindOnce(&MountPerformer::OnPrepareVaultForMigration,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

void MountPerformer::MountEphemeralDirectory(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  user_data_auth::PrepareEphemeralVaultRequest request;
  LOGIN_LOG(EVENT) << "Mount ephemeral directory";
  request.set_auth_session_id(context->GetAuthSessionId());
  UserDataAuthClient::Get()->PrepareEphemeralVault(
      request, base::BindOnce(&MountPerformer::OnPrepareEphemeralVault,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

void MountPerformer::MountGuestDirectory(std::unique_ptr<UserContext> context,
                                         AuthOperationCallback callback) {
  user_data_auth::PrepareGuestVaultRequest request;
  LOGIN_LOG(EVENT) << "Mount guest directory";
  UserDataAuthClient::Get()->PrepareGuestVault(
      request, base::BindOnce(&MountPerformer::OnPrepareGuestVault,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

void MountPerformer::RemoveUserDirectory(std::unique_ptr<UserContext> context,
                                         AuthOperationCallback callback) {
  user_data_auth::RemoveRequest request;
  request.set_auth_session_id(context->GetAuthSessionId());
  LOGIN_LOG(EVENT) << "Removing user directory";
  UserDataAuthClient::Get()->Remove(
      request,
      base::BindOnce(&MountPerformer::OnRemove, weak_factory_.GetWeakPtr(),
                     std::move(context), std::move(callback)));
}

// Unmounts all currently mounted directories.
void MountPerformer::UnmountDirectories(std::unique_ptr<UserContext> context,
                                        AuthOperationCallback callback) {
  user_data_auth::UnmountRequest request;
  LOGIN_LOG(EVENT) << "Unmounting all directories";
  UserDataAuthClient::Get()->Unmount(
      request,
      base::BindOnce(&MountPerformer::OnUnmount, weak_factory_.GetWeakPtr(),
                     std::move(context), std::move(callback)));
}

/// ---- private callbacks ----

void MountPerformer::OnCreatePersistentUser(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    absl::optional<user_data_auth::CreatePersistentUserReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (error != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOGIN_LOG(ERROR) << "CreatePersistentUser failed with error " << error;
    std::move(callback).Run(std::move(context), CryptohomeError{error});
    return;
  }
  CHECK(reply.has_value());
  std::move(callback).Run(std::move(context), absl::nullopt);
}

void MountPerformer::OnPrepareGuestVault(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    absl::optional<user_data_auth::PrepareGuestVaultReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (error != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOGIN_LOG(ERROR) << "PrepareGuestVault failed with error " << error;
    std::move(callback).Run(std::move(context), CryptohomeError{error});
    return;
  }
  CHECK(reply.has_value());
  context->SetUserIDHash(reply->sanitized_username());
  std::move(callback).Run(std::move(context), absl::nullopt);
}

void MountPerformer::OnPrepareEphemeralVault(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    absl::optional<user_data_auth::PrepareEphemeralVaultReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (error != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOGIN_LOG(ERROR) << "PrepareEphemeralVault failed with error " << error;
    std::move(callback).Run(std::move(context), CryptohomeError{error});
    return;
  }
  CHECK(reply.has_value());
  context->SetUserIDHash(reply->sanitized_username());
  std::move(callback).Run(std::move(context), absl::nullopt);
}

void MountPerformer::OnPreparePersistentVault(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    absl::optional<user_data_auth::PreparePersistentVaultReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (error != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOGIN_LOG(ERROR) << "PreparePersistentVault failed with error " << error;
    std::move(callback).Run(std::move(context), CryptohomeError{error});
    return;
  }
  CHECK(reply.has_value());
  context->SetUserIDHash(reply->sanitized_username());
  std::move(callback).Run(std::move(context), absl::nullopt);
}

void MountPerformer::OnPrepareVaultForMigration(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    absl::optional<user_data_auth::PrepareVaultForMigrationReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (error != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOGIN_LOG(ERROR) << "PrepareVaultForMigration failed with error " << error;
    std::move(callback).Run(std::move(context), CryptohomeError{error});
    return;
  }
  CHECK(reply.has_value());
  context->SetUserIDHash(reply->sanitized_username());
  std::move(callback).Run(std::move(context), absl::nullopt);
}

void MountPerformer::OnRemove(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    absl::optional<user_data_auth::RemoveReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (error != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOGIN_LOG(ERROR) << "Remove failed with error " << error;
    std::move(callback).Run(std::move(context), CryptohomeError{error});
    return;
  }
  CHECK(reply.has_value());
  context->ResetAuthSessionId();
  std::move(callback).Run(std::move(context), absl::nullopt);
}

void MountPerformer::OnUnmount(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    absl::optional<user_data_auth::UnmountReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (error != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOGIN_LOG(ERROR) << "Unmount failed with error" << error;
    std::move(callback).Run(std::move(context), CryptohomeError{error});
    return;
  }
  CHECK(reply.has_value());
  std::move(callback).Run(std::move(context), absl::nullopt);
}

}  // namespace ash
