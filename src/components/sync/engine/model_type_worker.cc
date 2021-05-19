// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/model_type_worker.h"

#include <stdint.h>

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/format_macros.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/base/hash_util.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/time.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/engine/bookmark_update_preprocessing.h"
#include "components/sync/engine/cancelation_signal.h"
#include "components/sync/engine/commit_contribution.h"
#include "components/sync/engine/commit_contribution_impl.h"
#include "components/sync/engine/cycle/entity_change_metric_recording.h"
#include "components/sync/engine/model_type_processor.h"
#include "components/sync/engine/sync_engine_switches.h"
#include "components/sync/protocol/proto_memory_estimations.h"

namespace syncer {

namespace {

const char kTimeUntilEncryptionKeyFoundHistogramPrefix[] =
    "Sync.ModelTypeTimeUntilEncryptionKeyFound.";
const char kUndecryptablePendingUpdatesDroppedHistogramPrefix[] =
    "Sync.ModelTypeUndecryptablePendingUpdatesDropped.";
const char kBlockedByUndecryptableUpdateHistogramName[] =
    "Sync.ModelTypeBlockedDueToUndecryptableUpdate";
const char kBlockedByUndecryptableUpdateButSomeKeysAvailableHistogramName[] =
    "Sync.ModelTypeBlockedDueToUndecryptableUpdate.SomeKeysAvailable";

const int kMinGuResponsesToIgnoreKey = 50;

void AdaptClientTagForFullUpdateData(ModelType model_type,
                                     syncer::EntityData* data) {
  // Server does not send any client tags for wallet data entities or offer data
  // entities. This code manually asks the bridge to create the client tags for
  // each entity, so that we can use ClientTagBasedModelTypeProcessor for
  // AUTOFILL_WALLET_DATA or AUTOFILL_WALLET_OFFER.
  if (data->parent_id == "0") {
    // Ignore the permanent root node as that one should have no client tag
    // hash.
    return;
  }
  DCHECK(!data->specifics.has_encrypted());
  if (model_type == AUTOFILL_WALLET_DATA) {
    DCHECK(data->specifics.has_autofill_wallet());
    data->client_tag_hash = ClientTagHash::FromUnhashed(
        AUTOFILL_WALLET_DATA, GetUnhashedClientTagFromAutofillWalletSpecifics(
                                  data->specifics.autofill_wallet()));
  } else if (model_type == AUTOFILL_WALLET_OFFER) {
    DCHECK(data->specifics.has_autofill_offer());
    data->client_tag_hash = ClientTagHash::FromUnhashed(
        AUTOFILL_WALLET_OFFER, GetUnhashedClientTagFromAutofillOfferSpecifics(
                                   data->specifics.autofill_offer()));
  } else {
    NOTREACHED();
  }
}

// Returns empty string if |entity| is not encrypted.
// TODO(crbug.com/1109221): Consider moving this to a util file and converting
// UpdateResponseData::encryption_key_name into a method that calls it. Consider
// returning a struct containing also the encrypted blob, which would make the
// code of PopulateUpdateResponseData() simpler.
std::string GetEncryptionKeyName(const sync_pb::SyncEntity& entity) {
  if (entity.deleted()) {
    return std::string();
  }
  // Passwords use their own legacy encryption scheme.
  if (entity.specifics().password().has_encrypted()) {
    return entity.specifics().password().encrypted().key_name();
  }
  if (entity.specifics().has_encrypted()) {
    return entity.specifics().encrypted().key_name();
  }
  return std::string();
}

// Attempts to decrypt the given specifics and return them in the |out|
// parameter. The cryptographer must know the decryption key, i.e.
// cryptographer.CanDecrypt(specifics.encrypted()) must return true.
//
// Returns false if the decryption failed. There are no guarantees about the
// contents of |out| when that happens.
//
// In theory, this should never fail. Only corrupt or invalid entries could
// cause this to fail, and no clients are known to create such entries. The
// failure case is an attempt to be defensive against bad input.
bool DecryptSpecifics(const Cryptographer& cryptographer,
                      const sync_pb::EntitySpecifics& in,
                      sync_pb::EntitySpecifics* out) {
  DCHECK(!in.has_password());
  DCHECK(in.has_encrypted());
  DCHECK(cryptographer.CanDecrypt(in.encrypted()));

  if (!cryptographer.Decrypt(in.encrypted(), out)) {
    DLOG(ERROR) << "Failed to decrypt a decryptable specifics";
    return false;
  }
  return true;
}

// Attempts to decrypt the given password specifics and return them in the
// |out| parameter. The cryptographer must know the decryption key, i.e.
// cryptographer.CanDecrypt(in.password().encrypted()) must return true.
//
// Returns false if the decryption failed. There are no guarantees about the
// contents of |out| when that happens.
//
// In theory, this should never fail. Only corrupt or invalid entries could
// cause this to fail, and no clients are known to create such entries. The
// failure case is an attempt to be defensive against bad input.
bool DecryptPasswordSpecifics(const Cryptographer& cryptographer,
                              const sync_pb::EntitySpecifics& in,
                              sync_pb::EntitySpecifics* out) {
  DCHECK(in.has_password());
  DCHECK(in.password().has_encrypted());
  DCHECK(cryptographer.CanDecrypt(in.password().encrypted()));

  if (!cryptographer.Decrypt(
          in.password().encrypted(),
          out->mutable_password()->mutable_client_only_encrypted_data())) {
    DLOG(ERROR) << "Failed to decrypt a decryptable password";
    return false;
  }
  return true;
}

}  // namespace

ModelTypeWorker::ModelTypeWorker(
    ModelType type,
    const sync_pb::ModelTypeState& initial_state,
    bool trigger_initial_sync,
    Cryptographer* cryptographer,
    PassphraseType passphrase_type,
    NudgeHandler* nudge_handler,
    std::unique_ptr<ModelTypeProcessor> model_type_processor,
    CancelationSignal* cancelation_signal)
    : type_(type),
      model_type_state_(initial_state),
      model_type_processor_(std::move(model_type_processor)),
      cryptographer_(cryptographer),
      passphrase_type_(passphrase_type),
      nudge_handler_(nudge_handler),
      min_gu_responses_to_ignore_key_(kMinGuResponsesToIgnoreKey),
      cancelation_signal_(cancelation_signal) {
  DCHECK(model_type_processor_);
  DCHECK(type_ != PASSWORDS || cryptographer_);

  if (!CommitOnlyTypes().Has(GetModelType())) {
    DCHECK_EQ(type, GetModelTypeFromSpecificsFieldNumber(
                        initial_state.progress_marker().data_type_id()));
  }

  // Request an initial sync if it hasn't been completed yet.
  if (trigger_initial_sync) {
    nudge_handler_->NudgeForInitialDownload(type_);
  }

  // This case handles the scenario where the processor has a serialized model
  // type state that has already done its initial sync, and is going to be
  // tracking metadata changes, however it does not have the most recent
  // encryption key name. The cryptographer was updated while the worker was not
  // around, and we're not going to receive the usual OnCryptographerChange() or
  // EncryptionAcceptedApplyUpdates() calls to drive this process.
  //
  // If |cryptographer_->CanEncrypt()| is false, all the rest of this logic can
  // be safely skipped, since |OnCryptographerChange()| must be called first
  // and things should be driven normally after that.
  //
  // If |model_type_state_.initial_sync_done()| is false, |model_type_state_|
  // may still need to be updated, since OnCryptographerChange() will never
  // happen, but we can assume ApplyUpdates(...) will push the state to the
  // processor, and we should not push it now. In fact, doing so now would
  // violate the processor's assumption that the first OnUpdateReceived is will
  // be changing initial sync done to true.
  if (cryptographer_ && cryptographer_->CanEncrypt() &&
      UpdateEncryptionKeyName() && model_type_state_.initial_sync_done()) {
    ApplyPendingUpdates();
  }
}

ModelTypeWorker::~ModelTypeWorker() {
  base::UmaHistogramCounts1000(
      std::string("Sync.UndecryptedEntitiesOnDataTypeDisabled.") +
          ModelTypeToHistogramSuffix(type_),
      entries_pending_decryption_.size());
  model_type_processor_->DisconnectSync();
}

ModelType ModelTypeWorker::GetModelType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return type_;
}

void ModelTypeWorker::EnableEncryption(Cryptographer* cryptographer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!cryptographer_);
  DCHECK(cryptographer);
  cryptographer_ = cryptographer;
  OnCryptographerChange();
}

void ModelTypeWorker::SetFallbackCryptographerForUma(
    Cryptographer* fallback_cryptographer_for_uma) {
  DCHECK(!fallback_cryptographer_for_uma_);
  DCHECK(fallback_cryptographer_for_uma);
  fallback_cryptographer_for_uma_ = fallback_cryptographer_for_uma;
}

void ModelTypeWorker::OnCryptographerChange() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(cryptographer_);
  UpdateEncryptionKeyName();
  DecryptStoredEntities();
  NudgeIfReadyToCommit();
}

void ModelTypeWorker::UpdatePassphraseType(PassphraseType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  passphrase_type_ = type;
}

// UpdateHandler implementation.
bool ModelTypeWorker::IsInitialSyncEnded() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return model_type_state_.initial_sync_done();
}

const sync_pb::DataTypeProgressMarker& ModelTypeWorker::GetDownloadProgress()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return model_type_state_.progress_marker();
}

const sync_pb::DataTypeContext& ModelTypeWorker::GetDataTypeContext() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return model_type_state_.type_context();
}

SyncerError ModelTypeWorker::ProcessGetUpdatesResponse(
    const sync_pb::DataTypeProgressMarker& progress_marker,
    const sync_pb::DataTypeContext& mutated_context,
    const SyncEntityList& applicable_updates,
    StatusController* status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const bool is_initial_sync = !model_type_state_.initial_sync_done();

  // TODO(rlarocque): Handle data type context conflicts.
  *model_type_state_.mutable_type_context() = mutated_context;
  *model_type_state_.mutable_progress_marker() = progress_marker;

  for (const sync_pb::SyncEntity* update_entity : applicable_updates) {
    RecordEntityChangeMetrics(
        type_, is_initial_sync
                   ? ModelTypeEntityChange::kRemoteInitialUpdate
                   : ModelTypeEntityChange::kRemoteNonInitialUpdate);

    if (update_entity->deleted()) {
      status->increment_num_tombstone_updates_downloaded_by(1);
      if (!is_initial_sync) {
        RecordEntityChangeMetrics(type_,
                                  ModelTypeEntityChange::kRemoteDeletion);
      }
    }

    UpdateResponseData response_data;
    switch (PopulateUpdateResponseData(cryptographer_, type_, *update_entity,
                                       &response_data)) {
      case SUCCESS:
        pending_updates_.push_back(std::move(response_data));
        // Override any previously undecryptable update for the same id.
        entries_pending_decryption_.erase(update_entity->id_string());
        break;
      case DECRYPTION_PENDING: {
        SyncRecordModelTypeUpdateDropReason(
            UpdateDropReason::kDecryptionPending, type_);

        const std::string& key_name = response_data.encryption_key_name;
        DCHECK(!key_name.empty());
        // If there's no entry for this unknown encryption key, create one.
        unknown_encryption_keys_by_name_.emplace(key_name,
                                                 UnknownEncryptionKeyInfo());

        const std::string& server_id = update_entity->id_string();
        if (ShouldIgnoreUpdatesEncryptedWith(key_name)) {
          // Don't queue the incoming update. If there's a queued entry for
          // |server_id|, don't clear it: outdated data is better than nothing.
          // Such entry should be encrypted with another key, since |key_name|'s
          // queued updates would've have been dropped by now.
          DCHECK(!base::Contains(entries_pending_decryption_, server_id) ||
                 GetEncryptionKeyName(entries_pending_decryption_[server_id]) !=
                     key_name);
          SyncRecordModelTypeUpdateDropReason(
              UpdateDropReason::kDecryptionPendingForTooLong, type_);
          break;
        }
        // Copy the sync entity for later decryption.
        entries_pending_decryption_[server_id] = *update_entity;
        break;
      }
      case FAILED_TO_DECRYPT:
        // Failed to decrypt the entity. Likely it is corrupt. Move on.
        SyncRecordModelTypeUpdateDropReason(UpdateDropReason::kFailedToDecrypt,
                                            type_);
        break;
    }
  }

  // Some updates pending decryption might have been overwritten by decryptable
  // ones. So some encryption keys may no longer fit the definition of unknown.
  RemoveKeysNoLongerUnknown();

  if (!cryptographer_ || cryptographer_->CanEncrypt()) {
    if (!entries_pending_decryption_.empty()) {
      RecordBlockedByUndecryptableUpdate();
    }

    // Encryption keys should've been known in this state.
    for (auto& key_and_info : unknown_encryption_keys_by_name_) {
      key_and_info.second.gu_responses_while_should_have_been_known++;
      // If the key is now missing for too long, drop pending updates encrypted
      // with it. This eventually unblocks a worker having undecryptable data.
      MaybeDropPendingUpdatesEncryptedWith(key_and_info.first);
    }
  }

  return SyncerError(SyncerError::SYNCER_OK);
}

// static
// |cryptographer| can be null.
// |response_data| must be not null.
ModelTypeWorker::DecryptionStatus ModelTypeWorker::PopulateUpdateResponseData(
    const Cryptographer* cryptographer,
    ModelType model_type,
    const sync_pb::SyncEntity& update_entity,
    UpdateResponseData* response_data) {
  syncer::EntityData data;

  // Deleted entities must use the default instance of EntitySpecifics in
  // order for EntityData to correctly reflect that they are deleted.
  const sync_pb::EntitySpecifics& specifics =
      update_entity.deleted() ? sync_pb::EntitySpecifics::default_instance()
                              : update_entity.specifics();
  bool specifics_were_encrypted = false;

  response_data->encryption_key_name = GetEncryptionKeyName(update_entity);
  if (specifics.password().has_encrypted()) {
    // Passwords use their own legacy encryption scheme.
    DCHECK(cryptographer);
    // TODO(crbug.com/516866): If we switch away from the password legacy
    // encryption, this method and DecryptStoredEntities() )should be already
    // ready for that change. Add unit test for this future-proofness.

    // Make sure the worker defers password entities if the encryption key
    // hasn't been received yet.
    if (!cryptographer->CanDecrypt(specifics.password().encrypted())) {
      return DECRYPTION_PENDING;
    }
    if (!DecryptPasswordSpecifics(*cryptographer, specifics, &data.specifics)) {
      return FAILED_TO_DECRYPT;
    }
    specifics_were_encrypted = true;
  } else if (specifics.has_encrypted()) {
    // Check if specifics are encrypted and try to decrypt if so.
    // Deleted entities should not be encrypted.
    DCHECK(!update_entity.deleted());
    if (!cryptographer || !cryptographer->CanDecrypt(specifics.encrypted())) {
      // Can't decrypt right now.
      return DECRYPTION_PENDING;
    }
    // Encrypted and we know the key.
    if (!DecryptSpecifics(*cryptographer, specifics, &data.specifics)) {
      return FAILED_TO_DECRYPT;
    }
    specifics_were_encrypted = true;
  } else {
    // No encryption.
    data.specifics = specifics;
  }

  response_data->response_version = update_entity.version();
  // Prepare the message for the model thread.
  data.id = update_entity.id_string();
  data.client_tag_hash =
      ClientTagHash::FromHashed(update_entity.client_defined_unique_tag());
  data.creation_time = ProtoTimeToTime(update_entity.ctime());
  data.modification_time = ProtoTimeToTime(update_entity.mtime());
  data.name = update_entity.name();
  data.is_folder = update_entity.folder();
  data.parent_id = update_entity.parent_id_string();
  data.server_defined_unique_tag = update_entity.server_defined_unique_tag();

  // Populate |originator_cache_guid| and |originator_client_item_id|. This is
  // currently relevant only for bookmarks.
  data.originator_cache_guid = update_entity.originator_cache_guid();
  data.originator_client_item_id = update_entity.originator_client_item_id();

  // Adapt the update for compatibility.
  if (model_type == BOOKMARKS) {
    AdaptUniquePositionForBookmark(update_entity, &data);
    AdaptTitleForBookmark(update_entity, &data.specifics,
                          specifics_were_encrypted);
    data.is_bookmark_guid_in_specifics_preprocessed =
        AdaptGuidForBookmark(update_entity, &data.specifics);
  } else if (model_type == AUTOFILL_WALLET_DATA ||
             model_type == AUTOFILL_WALLET_OFFER) {
    AdaptClientTagForFullUpdateData(model_type, &data);
  }

  response_data->entity = std::move(data);
  return SUCCESS;
}

void ModelTypeWorker::ApplyUpdates(StatusController* status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Indicate to the processor that the initial download is done. The initial
  // sync technically isn't done yet but by the time this value is persisted to
  // disk on the model thread it will be.
  //
  // This should be mostly relevant for the call from ApplyUpdates(), but in
  // rare cases we may end up receiving initial updates outside configuration
  // cycles (e.g. polling cycles).
  model_type_state_.set_initial_sync_done(true);
  // Download cycle is done, pass all updates to the processor.
  ApplyPendingUpdates();
}

void ModelTypeWorker::EncryptionAcceptedMaybeApplyUpdates() {
  DCHECK(cryptographer_);
  DCHECK(cryptographer_->CanEncrypt());

  // Only push the encryption to the processor if we're already connected.
  // Otherwise this information can wait for the initial sync's first apply.
  if (model_type_state_.initial_sync_done()) {
    // Reuse ApplyUpdates(...) to get its DCHECKs as well.
    ApplyUpdates(nullptr);
  }
}

void ModelTypeWorker::ApplyPendingUpdates() {
  if (BlockForEncryption())
    return;

  DCHECK(entries_pending_decryption_.empty());

  DVLOG(1) << ModelTypeToString(type_) << ": "
           << base::StringPrintf("Delivering %" PRIuS " applicable updates.",
                                 pending_updates_.size());

  // Deduplicate updates first based on server ids, which is the only legit
  // source of duplicates, specially due to pagination.
  DeduplicatePendingUpdatesBasedOnServerId();

  // As extra precaution, and although it shouldn't be necessary without a
  // misbehaving server, deduplicate based on client tags and originator item
  // IDs. This allows further code to use DCHECKs without relying on external
  // behavior.
  DeduplicatePendingUpdatesBasedOnClientTagHash();
  DeduplicatePendingUpdatesBasedOnOriginatorClientItemId();

  model_type_processor_->OnUpdateReceived(model_type_state_,
                                          std::move(pending_updates_));

  pending_updates_.clear();
}

void ModelTypeWorker::NudgeForCommit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  has_local_changes_state_ = kNewlyNudgedLocalChanges;
  NudgeIfReadyToCommit();
}

void ModelTypeWorker::NudgeIfReadyToCommit() {
  // TODO(crbug.com/1188034): |kNoNudgedLocalChanges| is used to keep the
  // existing behaviour. But perhaps there is no need to nudge for commit if all
  // known changes are already in flight.
  if (has_local_changes_state_ != kNoNudgedLocalChanges && CanCommitItems())
    nudge_handler_->NudgeForCommit(GetModelType());
}

// CommitContributor implementation.
std::unique_ptr<CommitContribution> ModelTypeWorker::GetContribution(
    size_t max_entries) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(model_type_state_.initial_sync_done());
  // Early return if type is not ready to commit (initial sync isn't done or
  // cryptographer has pending keys).
  if (!CanCommitItems())
    return std::unique_ptr<CommitContribution>();

  // Client shouldn't be committing data to server when it hasn't processed all
  // updates it received.
  DCHECK(entries_pending_decryption_.empty());

  // Pull local changes from the processor (in the model thread/sequence). Note
  // that this takes place independently of nudges (i.e.
  // |has_local_changes_state_|), in case the processor decided a local change
  // was not worth a nudge.
  scoped_refptr<GetLocalChangesRequest> request =
      base::MakeRefCounted<GetLocalChangesRequest>(cancelation_signal_);
  model_type_processor_->GetLocalChanges(
      max_entries,
      base::BindOnce(&GetLocalChangesRequest::SetResponse, request));
  request->WaitForResponseOrCancelation();
  CommitRequestDataList response;
  if (!request->WasCancelled())
    response = request->ExtractResponse();
  if (response.empty()) {
    has_local_changes_state_ = kNoNudgedLocalChanges;
    return std::unique_ptr<CommitContribution>();
  }

  DCHECK(response.size() <= max_entries);
  if (response.size() < max_entries) {
    // In case when response.size() equals to |max_entries|, there will be
    // another commit request (see CommitProcessor::GatherCommitContributions).
    // Hence, in general it should be normal if |has_local_changes_state_| is
    // |kNewlyNudgedLocalChanges| (even if there are no more items in the
    // processor). In other words, |kAllNudgedLocalChangesInFlight| means that
    // there might not be another commit request in the current sync cycle (but
    // still possible if some other data type contributes |max_entities|).
    has_local_changes_state_ = kAllNudgedLocalChangesInFlight;
  }

  return std::make_unique<CommitContributionImpl>(
      GetModelType(), model_type_state_.type_context(), std::move(response),
      base::BindOnce(&ModelTypeWorker::OnCommitResponse,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&ModelTypeWorker::OnFullCommitFailure,
                     weak_ptr_factory_.GetWeakPtr()),
      cryptographer_, passphrase_type_, CommitOnlyTypes().Has(GetModelType()));
}

bool ModelTypeWorker::HasLocalChangesForTest() const {
  return has_local_changes_state_ != kNoNudgedLocalChanges;
}

void ModelTypeWorker::OnCommitResponse(
    const CommitResponseDataList& committed_response_list,
    const FailedCommitResponseDataList& error_response_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Send the responses back to the model thread. It needs to know which
  // items have been successfully committed (it can save that information in
  // permanent storage) and which failed (it can e.g. notify the user).
  model_type_processor_->OnCommitCompleted(
      model_type_state_, committed_response_list, error_response_list);

  if (has_local_changes_state_ == kAllNudgedLocalChangesInFlight) {
    // There are no new nudged changes since last commit.
    has_local_changes_state_ = kNoNudgedLocalChanges;
  }
}

void ModelTypeWorker::OnFullCommitFailure(SyncCommitError commit_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  model_type_processor_->OnCommitFailed(commit_error);
}

size_t ModelTypeWorker::EstimateMemoryUsage() const {
  using base::trace_event::EstimateMemoryUsage;
  size_t memory_usage = 0;
  memory_usage += EstimateMemoryUsage(model_type_state_);
  memory_usage += EstimateMemoryUsage(entries_pending_decryption_);
  memory_usage += EstimateMemoryUsage(pending_updates_);
  return memory_usage;
}

base::WeakPtr<ModelTypeWorker> ModelTypeWorker::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool ModelTypeWorker::IsTypeInitialized() const {
  return model_type_state_.initial_sync_done();
}

bool ModelTypeWorker::CanCommitItems() const {
  // We can only commit if we've received the initial update response and aren't
  // blocked by missing encryption keys.
  return IsTypeInitialized() && !BlockForEncryption();
}

bool ModelTypeWorker::BlockForEncryption() const {
  if (!entries_pending_decryption_.empty())
    return true;

  // Should be using encryption, but we do not have the keys.
  return cryptographer_ && !cryptographer_->CanEncrypt();
}

bool ModelTypeWorker::UpdateEncryptionKeyName() {
  const std::string& new_key_name =
      cryptographer_->GetDefaultEncryptionKeyName();
  const std::string& old_key_name = model_type_state_.encryption_key_name();
  if (old_key_name == new_key_name) {
    return false;
  }

  DVLOG(1) << ModelTypeToString(type_) << ": Updating encryption key "
           << old_key_name << " -> " << new_key_name;
  model_type_state_.set_encryption_key_name(new_key_name);
  return true;
}

void ModelTypeWorker::DecryptStoredEntities() {
  for (auto it = entries_pending_decryption_.begin();
       it != entries_pending_decryption_.end();) {
    const sync_pb::SyncEntity& encrypted_update = it->second;

    UpdateResponseData response_data;
    switch (PopulateUpdateResponseData(cryptographer_, type_, encrypted_update,
                                       &response_data)) {
      case SUCCESS:
        pending_updates_.push_back(std::move(response_data));
        it = entries_pending_decryption_.erase(it);
        break;
      case DECRYPTION_PENDING:
        // Still cannot decrypt, move on and keep this one for later.
        ++it;
        break;
      case FAILED_TO_DECRYPT:
        // Decryption error should be permanent (e.g. corrupt data), since
        // decryption keys are up-to-date. Let's ignore this update to avoid
        // blocking other updates.
        it = entries_pending_decryption_.erase(it);
        break;
    }
  }

  // Note this can perfectly contain keys that were encrypting corrupt updates
  // (FAILED_TO_DECRYPT above); all that matters is the key was found.
  const std::vector<UnknownEncryptionKeyInfo> newly_found_keys =
      RemoveKeysNoLongerUnknown();
  for (const UnknownEncryptionKeyInfo& newly_found_key : newly_found_keys) {
    // Don't record UMA for the dominant case where the key was only unknown
    // while the cryptographer was pending external interaction.
    if (newly_found_key.gu_responses_while_should_have_been_known > 0) {
      base::UmaHistogramCounts1000(
          base::StrCat({kTimeUntilEncryptionKeyFoundHistogramPrefix,
                        ModelTypeToHistogramSuffix(type_)}),
          newly_found_key.gu_responses_while_should_have_been_known);
    }
  }
}

void ModelTypeWorker::DeduplicatePendingUpdatesBasedOnServerId() {
  UpdateResponseDataList candidates;
  pending_updates_.swap(candidates);
  pending_updates_.reserve(candidates.size());

  std::map<std::string, size_t> id_to_index;
  for (UpdateResponseData& candidate : candidates) {
    if (candidate.entity.id.empty()) {
      continue;
    }
    // Try to insert. If we already saw an item with the same server id,
    // this will fail but give us its iterator.
    auto it_and_success =
        id_to_index.emplace(candidate.entity.id, pending_updates_.size());
    if (it_and_success.second) {
      // New server id, append at the end. Note that we already inserted
      // the correct index (|pending_updates_.size()|) above.
      pending_updates_.push_back(std::move(candidate));
    } else {
      // Duplicate! Overwrite the existing item.
      size_t existing_index = it_and_success.first->second;
      pending_updates_[existing_index] = std::move(candidate);
    }
  }
}

void ModelTypeWorker::DeduplicatePendingUpdatesBasedOnClientTagHash() {
  UpdateResponseDataList candidates;
  pending_updates_.swap(candidates);
  pending_updates_.reserve(candidates.size());

  std::map<ClientTagHash, size_t> tag_to_index;
  for (UpdateResponseData& candidate : candidates) {
    // Items with empty client tag hash just get passed through.
    if (candidate.entity.client_tag_hash.value().empty()) {
      pending_updates_.push_back(std::move(candidate));
      continue;
    }
    // Try to insert. If we already saw an item with the same client tag hash,
    // this will fail but give us its iterator.
    auto it_and_success = tag_to_index.emplace(candidate.entity.client_tag_hash,
                                               pending_updates_.size());
    if (it_and_success.second) {
      // New client tag hash, append at the end. Note that we already inserted
      // the correct index (|pending_updates_.size()|) above.
      pending_updates_.push_back(std::move(candidate));
    } else {
      // Duplicate! Overwrite the existing item.
      size_t existing_index = it_and_success.first->second;
      pending_updates_[existing_index] = std::move(candidate);
    }
  }
}

void ModelTypeWorker::DeduplicatePendingUpdatesBasedOnOriginatorClientItemId() {
  UpdateResponseDataList candidates;
  pending_updates_.swap(candidates);
  pending_updates_.reserve(candidates.size());

  std::map<std::string, size_t> id_to_index;
  for (UpdateResponseData& candidate : candidates) {
    // Entities with an item ID that is not a GUID just get passed through
    // without deduplication, which is the case for all datatypes except
    // bookmarks, as well as bookmarks created before 2015, when the item ID was
    // not globally unique across clients.
    if (!base::IsValidGUID(candidate.entity.originator_client_item_id)) {
      pending_updates_.push_back(std::move(candidate));
      continue;
    }
    // Try to insert. If we already saw an item with the same originator item
    // ID, this will fail but give us its iterator.
    auto it_and_success = id_to_index.emplace(
        base::ToLowerASCII(candidate.entity.originator_client_item_id),
        pending_updates_.size());
    if (it_and_success.second) {
      // New item ID, append at the end. Note that we already inserted the
      // correct index (|pending_updates_.size()|) above.
      pending_updates_.push_back(std::move(candidate));
    } else {
      // Duplicate! Overwrite the existing item.
      size_t existing_index = it_and_success.first->second;
      pending_updates_[existing_index] = std::move(candidate);
    }
  }
}

bool ModelTypeWorker::ShouldIgnoreUpdatesEncryptedWith(
    const std::string& key_name) {
  if (!base::Contains(unknown_encryption_keys_by_name_, key_name)) {
    return false;
  }
  if (unknown_encryption_keys_by_name_.at(key_name)
          .gu_responses_while_should_have_been_known <
      min_gu_responses_to_ignore_key_) {
    return false;
  }
  return base::FeatureList::IsEnabled(
      switches::kIgnoreSyncEncryptionKeysLongMissing);
}

void ModelTypeWorker::MaybeDropPendingUpdatesEncryptedWith(
    const std::string& key_name) {
  if (!ShouldIgnoreUpdatesEncryptedWith(key_name)) {
    return;
  }

  size_t updates_before_dropping = entries_pending_decryption_.size();
  base::EraseIf(entries_pending_decryption_, [&](const auto& id_and_update) {
    return key_name == GetEncryptionKeyName(id_and_update.second);
  });

  // If updates were dropped, record how many.
  if (entries_pending_decryption_.size() < updates_before_dropping) {
    base::UmaHistogramCounts1000(
        base::StrCat({kUndecryptablePendingUpdatesDroppedHistogramPrefix,
                      ModelTypeToHistogramSuffix(type_)}),
        updates_before_dropping - entries_pending_decryption_.size());
  }
}

std::vector<ModelTypeWorker::UnknownEncryptionKeyInfo>
ModelTypeWorker::RemoveKeysNoLongerUnknown() {
  std::set<std::string> keys_blocking_updates;
  for (const auto& id_and_update : entries_pending_decryption_) {
    const std::string key_name = GetEncryptionKeyName(id_and_update.second);
    DCHECK(!key_name.empty());
    keys_blocking_updates.insert(key_name);
  }

  std::vector<ModelTypeWorker::UnknownEncryptionKeyInfo> removed_keys;
  base::EraseIf(
      unknown_encryption_keys_by_name_, [&](const auto& key_and_info) {
        if (base::Contains(keys_blocking_updates, key_and_info.first)) {
          return false;
        }
        removed_keys.push_back(key_and_info.second);
        return true;
      });

  return removed_keys;
}

void ModelTypeWorker::RecordBlockedByUndecryptableUpdate() {
  base::UmaHistogramEnumeration(kBlockedByUndecryptableUpdateHistogramName,
                                ModelTypeHistogramValue(type_));

  if (cryptographer_ || !fallback_cryptographer_for_uma_) {
    return;
  }

  // There's no real |cryptographer_|, but maybe
  // |fallback_cryptographer_for_uma_| can decrypt the data.
  for (const auto& id_and_pending_update : entries_pending_decryption_) {
    UpdateResponseData ignored;
    if (PopulateUpdateResponseData(fallback_cryptographer_for_uma_, type_,
                                   id_and_pending_update.second,
                                   &ignored) == SUCCESS) {
      base::UmaHistogramEnumeration(
          kBlockedByUndecryptableUpdateButSomeKeysAvailableHistogramName,
          ModelTypeHistogramValue(type_));
      break;
    }
  }
}

GetLocalChangesRequest::GetLocalChangesRequest(
    CancelationSignal* cancelation_signal)
    : cancelation_signal_(cancelation_signal),
      response_accepted_(base::WaitableEvent::ResetPolicy::MANUAL,
                         base::WaitableEvent::InitialState::NOT_SIGNALED) {}

GetLocalChangesRequest::~GetLocalChangesRequest() = default;

void GetLocalChangesRequest::OnCancelationSignalReceived() {
  response_accepted_.Signal();
}

void GetLocalChangesRequest::WaitForResponseOrCancelation() {
  if (!cancelation_signal_->TryRegisterHandler(this)) {
    return;
  }

  {
    base::ScopedAllowBaseSyncPrimitives allow_wait;
    response_accepted_.Wait();
  }

  cancelation_signal_->UnregisterHandler(this);
}

void GetLocalChangesRequest::SetResponse(
    CommitRequestDataList&& local_changes) {
  response_ = std::move(local_changes);
  response_accepted_.Signal();
}

bool GetLocalChangesRequest::WasCancelled() {
  return cancelation_signal_->IsSignalled();
}

CommitRequestDataList&& GetLocalChangesRequest::ExtractResponse() {
  return std::move(response_);
}

}  // namespace syncer
