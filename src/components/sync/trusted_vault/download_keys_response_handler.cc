// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/download_keys_response_handler.h"

#include <map>
#include <utility>

#include "components/sync/protocol/vault.pb.h"
#include "components/sync/trusted_vault/proto_string_bytes_conversion.h"
#include "components/sync/trusted_vault/securebox.h"
#include "components/sync/trusted_vault/trusted_vault_crypto.h"
#include "components/sync/trusted_vault/trusted_vault_server_constants.h"

namespace syncer {

namespace {

struct ExtractedSharedKey {
  int version;
  std::vector<uint8_t> trusted_vault_key;
  std::vector<uint8_t> rotation_proof;
};

const sync_pb::SecurityDomainMember::SecurityDomainMembership*
FindSyncMembership(const sync_pb::SecurityDomainMember& member) {
  for (const auto& membership : member.memberships()) {
    if (membership.security_domain() == kSyncSecurityDomainName) {
      return &membership;
    }
  }
  return nullptr;
}

// Extracts (decrypts |wrapped_key| and converts to ExtractedSharedKey) shared
// keys from |membership| and sorts them by version.
std::vector<ExtractedSharedKey> ExtractAndSortSharedKeys(
    const sync_pb::SecurityDomainMember::SecurityDomainMembership& membership,
    const SecureBoxPrivateKey& member_private_key) {
  std::map<int, ExtractedSharedKey> epoch_to_extracted_key;
  for (const sync_pb::SharedMemberKey& shared_key : membership.keys()) {
    absl::optional<std::vector<uint8_t>> decrypted_key =
        DecryptTrustedVaultWrappedKey(
            member_private_key, ProtoStringToBytes(shared_key.wrapped_key()));
    if (!decrypted_key.has_value()) {
      // Decryption failed.
      return std::vector<ExtractedSharedKey>();
    }
    epoch_to_extracted_key[shared_key.epoch()].version = shared_key.epoch();
    epoch_to_extracted_key[shared_key.epoch()].trusted_vault_key =
        *decrypted_key;
  }
  for (const sync_pb::RotationProof& rotation_proof :
       membership.rotation_proofs()) {
    if (epoch_to_extracted_key.count(rotation_proof.new_epoch()) == 0) {
      // There is no shared key corresponding to rotation proof. In theory it
      // shouldn't happen, but it's safe to ignore.
      continue;
    }
    epoch_to_extracted_key[rotation_proof.new_epoch()].rotation_proof =
        ProtoStringToBytes(rotation_proof.rotation_proof());
  }

  std::vector<ExtractedSharedKey> result;
  for (const auto& epoch_and_extracted_key : epoch_to_extracted_key) {
    result.push_back(epoch_and_extracted_key.second);
  }
  return result;
}

// |sorted_keys| must be non-empty and sorted by version. Returns new keys, a
// key is new if it has higher version than
// |last_known_trusted_vault_key_and_version|.
std::vector<ExtractedSharedKey> GetNewKeys(
    const std::vector<ExtractedSharedKey>& sorted_keys,
    const TrustedVaultKeyAndVersion& last_known_trusted_vault_key_and_version) {
  DCHECK(!sorted_keys.empty());
  auto new_keys_start_it = std::find_if(
      sorted_keys.begin(), sorted_keys.end(),
      [&last_known_trusted_vault_key_and_version](
          const ExtractedSharedKey& key) {
        return key.version > last_known_trusted_vault_key_and_version.version;
      });

  return std::vector<ExtractedSharedKey>(new_keys_start_it, sorted_keys.end());
}

// Validates |rotation_proof| starting from the key next to
// last known trusted vault key.
bool IsValidKeyChain(
    const std::vector<ExtractedSharedKey>& key_chain,
    const TrustedVaultKeyAndVersion& last_known_trusted_vault_key_and_version) {
  DCHECK(!key_chain.empty());
  int last_valid_key_version = last_known_trusted_vault_key_and_version.version;
  std::vector<uint8_t> last_valid_key =
      last_known_trusted_vault_key_and_version.key;
  for (const ExtractedSharedKey& next_key : key_chain) {
    if (next_key.version <= last_valid_key_version) {
      continue;
    }
    if (next_key.version != last_valid_key_version + 1) {
      // Missing intermediate key.
      return false;
    }

    if (!VerifyRotationProof(/*trusted_vault_key=*/next_key.trusted_vault_key,
                             /*prev_trusted_vault_key=*/last_valid_key,
                             next_key.rotation_proof)) {
      // |rotation_proof| isn't valid.
      return false;
    }

    last_valid_key_version = next_key.version;
    last_valid_key = next_key.trusted_vault_key;
  }

  return true;
}

}  // namespace

DownloadKeysResponseHandler::ProcessedResponse::ProcessedResponse(
    TrustedVaultDownloadKeysStatus status)
    : status(status), last_key_version(0) {}

DownloadKeysResponseHandler::ProcessedResponse::ProcessedResponse(
    TrustedVaultDownloadKeysStatus status,
    std::vector<std::vector<uint8_t>> new_keys,
    int last_key_version)
    : status(status), new_keys(new_keys), last_key_version(last_key_version) {}

DownloadKeysResponseHandler::ProcessedResponse::ProcessedResponse(
    const ProcessedResponse& other) = default;

DownloadKeysResponseHandler::ProcessedResponse&
DownloadKeysResponseHandler::ProcessedResponse::operator=(
    const ProcessedResponse& other) = default;

DownloadKeysResponseHandler::ProcessedResponse::~ProcessedResponse() = default;

DownloadKeysResponseHandler::DownloadKeysResponseHandler(
    const TrustedVaultKeyAndVersion& last_trusted_vault_key_and_version,
    std::unique_ptr<SecureBoxKeyPair> device_key_pair)
    : last_trusted_vault_key_and_version_(last_trusted_vault_key_and_version),
      device_key_pair_(std::move(device_key_pair)) {
  DCHECK(device_key_pair_);
}

DownloadKeysResponseHandler::~DownloadKeysResponseHandler() = default;

DownloadKeysResponseHandler::ProcessedResponse
DownloadKeysResponseHandler::ProcessResponse(
    TrustedVaultRequest::HttpStatus http_status,
    const std::string& response_body) const {
  switch (http_status) {
    case TrustedVaultRequest::HttpStatus::kSuccess:
      break;
    case TrustedVaultRequest::HttpStatus::kNotFound:
      return ProcessedResponse(
          /*status=*/TrustedVaultDownloadKeysStatus::
              kMemberNotFoundOrCorrupted);
    case TrustedVaultRequest::HttpStatus::kAccessTokenFetchingFailure:
      return ProcessedResponse(
          /*status=*/TrustedVaultDownloadKeysStatus::
              kAccessTokenFetchingFailure);
    case TrustedVaultRequest::HttpStatus::kBadRequest:
    case TrustedVaultRequest::HttpStatus::kConflict:
    case TrustedVaultRequest::HttpStatus::kOtherError:
      return ProcessedResponse(
          /*status=*/TrustedVaultDownloadKeysStatus::kOtherError);
  }

  sync_pb::SecurityDomainMember member;
  if (!member.ParseFromString(response_body)) {
    return ProcessedResponse(
        /*status=*/TrustedVaultDownloadKeysStatus::kOtherError);
  }

  // TODO(crbug.com/1113598): consider validation of member public key.
  const sync_pb::SecurityDomainMember::SecurityDomainMembership* membership =
      FindSyncMembership(member);
  if (!membership) {
    // Member is not in sync security domain.
    return ProcessedResponse(
        /*status=*/TrustedVaultDownloadKeysStatus::kMemberNotFoundOrCorrupted);
  }

  std::vector<ExtractedSharedKey> extracted_keys =
      ExtractAndSortSharedKeys(*membership, device_key_pair_->private_key());
  if (extracted_keys.empty()) {
    // |current_member| doesn't have any keys, should be treated as not
    // registered member.
    return ProcessedResponse(
        /*status=*/TrustedVaultDownloadKeysStatus::kMemberNotFoundOrCorrupted);
  }

  if (!IsValidKeyChain(extracted_keys, last_trusted_vault_key_and_version_)) {
    // Data corresponding to |current_member| is corrupted or
    // |last_trusted_vault_key_and_version_| is too old.
    return ProcessedResponse(
        /*status=*/TrustedVaultDownloadKeysStatus::
            kKeyProofsVerificationFailed);
  }

  std::vector<ExtractedSharedKey> new_keys =
      GetNewKeys(extracted_keys, last_trusted_vault_key_and_version_);
  if (new_keys.empty()) {
    return ProcessedResponse(
        /*status=*/TrustedVaultDownloadKeysStatus::kNoNewKeys);
  }
  std::vector<std::vector<uint8_t>> new_trusted_vault_keys;
  for (auto& key : new_keys) {
    new_trusted_vault_keys.push_back(key.trusted_vault_key);
  }
  return ProcessedResponse(/*status=*/TrustedVaultDownloadKeysStatus::kSuccess,
                           new_trusted_vault_keys,
                           /*last_key_version=*/new_keys.back().version);
}

}  // namespace syncer
