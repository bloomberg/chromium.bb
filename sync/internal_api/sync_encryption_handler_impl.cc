// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/sync_encryption_handler_impl.h"

#include <queue>
#include <string>

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "base/tracked_objects.h"
#include "base/metrics/histogram.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/internal_api/public/read_transaction.h"
#include "sync/internal_api/public/user_share.h"
#include "sync/internal_api/public/util/experiments.h"
#include "sync/internal_api/public/write_node.h"
#include "sync/internal_api/public/write_transaction.h"
#include "sync/internal_api/public/util/sync_string_conversions.h"
#include "sync/protocol/encryption.pb.h"
#include "sync/protocol/nigori_specifics.pb.h"
#include "sync/protocol/sync.pb.h"
#include "sync/syncable/base_transaction.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/entry.h"
#include "sync/syncable/nigori_util.h"
#include "sync/util/cryptographer.h"
#include "sync/util/time.h"

namespace syncer {

namespace {

// The maximum number of times we will automatically overwrite the nigori node
// because the encryption keys don't match (per chrome instantiation).
// We protect ourselves against nigori rollbacks, but it's possible two
// different clients might have contrasting view of what the nigori node state
// should be, in which case they might ping pong (see crbug.com/119207).
static const int kNigoriOverwriteLimit = 10;

// The new passphrase state is sufficient to determine whether a nigori node
// is migrated to support keystore encryption. In addition though, we also
// want to verify the conditions for proper keystore encryption functionality.
// 1. Passphrase state is set.
// 2. Migration time is set.
// 3. Frozen keybag is true
// 4. If passphrase state is keystore, keystore_decryptor_token is set.
bool IsNigoriMigratedToKeystore(const sync_pb::NigoriSpecifics& nigori) {
  if (!nigori.has_passphrase_type())
    return false;
  if (!nigori.has_keystore_migration_time())
    return false;
  if (!nigori.keybag_is_frozen())
    return false;
  if (nigori.passphrase_type() ==
          sync_pb::NigoriSpecifics::IMPLICIT_PASSPHRASE)
    return false;
  if (nigori.passphrase_type() ==
          sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE &&
      nigori.keystore_decryptor_token().blob().empty())
    return false;
  if (!nigori.has_keystore_migration_time())
    return false;
  return true;
}

PassphraseType ProtoPassphraseTypeToEnum(
    sync_pb::NigoriSpecifics::PassphraseType type) {
  switch(type) {
    case sync_pb::NigoriSpecifics::IMPLICIT_PASSPHRASE:
      return IMPLICIT_PASSPHRASE;
    case sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE:
      return KEYSTORE_PASSPHRASE;
    case sync_pb::NigoriSpecifics::CUSTOM_PASSPHRASE:
      return CUSTOM_PASSPHRASE;
    case sync_pb::NigoriSpecifics::FROZEN_IMPLICIT_PASSPHRASE:
      return FROZEN_IMPLICIT_PASSPHRASE;
    default:
      NOTREACHED();
      return IMPLICIT_PASSPHRASE;
  };
}

sync_pb::NigoriSpecifics::PassphraseType
EnumPassphraseTypeToProto(PassphraseType type) {
  switch(type) {
    case IMPLICIT_PASSPHRASE:
      return sync_pb::NigoriSpecifics::IMPLICIT_PASSPHRASE;
    case KEYSTORE_PASSPHRASE:
      return sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE;
    case CUSTOM_PASSPHRASE:
      return sync_pb::NigoriSpecifics::CUSTOM_PASSPHRASE;
    case FROZEN_IMPLICIT_PASSPHRASE:
      return sync_pb::NigoriSpecifics::FROZEN_IMPLICIT_PASSPHRASE;
    default:
      NOTREACHED();
      return sync_pb::NigoriSpecifics::IMPLICIT_PASSPHRASE;;
  };
}

bool IsExplicitPassphrase(PassphraseType type) {
  return type == CUSTOM_PASSPHRASE || type == FROZEN_IMPLICIT_PASSPHRASE;
}

}  // namespace

SyncEncryptionHandlerImpl::Vault::Vault(
    Encryptor* encryptor,
    ModelTypeSet encrypted_types)
    : cryptographer(encryptor),
      encrypted_types(encrypted_types) {
}

SyncEncryptionHandlerImpl::Vault::~Vault() {
}

SyncEncryptionHandlerImpl::SyncEncryptionHandlerImpl(
    UserShare* user_share,
    Encryptor* encryptor,
    const std::string& restored_key_for_bootstrapping,
    const std::string& restored_keystore_key_for_bootstrapping)
    : weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      user_share_(user_share),
      vault_unsafe_(encryptor, SensitiveTypes()),
      encrypt_everything_(false),
      passphrase_type_(IMPLICIT_PASSPHRASE),
      keystore_key_(restored_keystore_key_for_bootstrapping),
      nigori_overwrite_count_(0),
      migration_time_ms_(0) {
  // We only bootstrap the user provided passphrase. The keystore key is handled
  // at Init time once we're sure the nigori is downloaded.
  vault_unsafe_.cryptographer.Bootstrap(restored_key_for_bootstrapping);
}

SyncEncryptionHandlerImpl::~SyncEncryptionHandlerImpl() {}

void SyncEncryptionHandlerImpl::AddObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!observers_.HasObserver(observer));
  observers_.AddObserver(observer);
}

void SyncEncryptionHandlerImpl::RemoveObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(observers_.HasObserver(observer));
  observers_.RemoveObserver(observer);
}

void SyncEncryptionHandlerImpl::Init() {
  DCHECK(thread_checker_.CalledOnValidThread());
  WriteTransaction trans(FROM_HERE, user_share_);
  WriteNode node(&trans);

  if (node.InitByTagLookup(kNigoriTag) != BaseNode::INIT_OK)
    return;
  if (!ApplyNigoriUpdateImpl(node.GetNigoriSpecifics(),
                             trans.GetWrappedTrans())) {
    WriteEncryptionStateToNigori(&trans);
  }

  // Always trigger an encrypted types and cryptographer state change event at
  // init time so observers get the initial values.
  FOR_EACH_OBSERVER(
      Observer, observers_,
      OnEncryptedTypesChanged(
          UnlockVault(trans.GetWrappedTrans()).encrypted_types,
          encrypt_everything_));
  FOR_EACH_OBSERVER(
      SyncEncryptionHandler::Observer,
      observers_,
      OnCryptographerStateChanged(
          &UnlockVaultMutable(trans.GetWrappedTrans())->cryptographer));

  // If the cryptographer is not ready (either it has pending keys or we
  // failed to initialize it), we don't want to try and re-encrypt the data.
  // If we had encrypted types, the DataTypeManager will block, preventing
  // sync from happening until the the passphrase is provided.
  if (UnlockVault(trans.GetWrappedTrans()).cryptographer.is_ready())
    ReEncryptEverything(&trans);
}

void SyncEncryptionHandlerImpl::SetEncryptionPassphrase(
    const std::string& passphrase,
    bool is_explicit) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // We do not accept empty passphrases.
  if (passphrase.empty()) {
    NOTREACHED() << "Cannot encrypt with an empty passphrase.";
    return;
  }

  // All accesses to the cryptographer are protected by a transaction.
  WriteTransaction trans(FROM_HERE, user_share_);
  KeyParams key_params = {"localhost", "dummy", passphrase};
  WriteNode node(&trans);
  if (node.InitByTagLookup(kNigoriTag) != BaseNode::INIT_OK) {
    NOTREACHED();
    return;
  }

  Cryptographer* cryptographer =
      &UnlockVaultMutable(trans.GetWrappedTrans())->cryptographer;

  // Once we've migrated to keystore, the only way to set a passphrase for
  // encryption is to set a custom passphrase.
  if (IsNigoriMigratedToKeystore(node.GetNigoriSpecifics())) {
    if (!is_explicit) {
      DCHECK(cryptographer->is_ready());
      // The user is setting a new implicit passphrase. At this point we don't
      // care, so drop it on the floor. This is safe because if we have a
      // migrated nigori node, then we don't need to create an initial
      // encryption key.
      LOG(WARNING) << "Ignoring new implicit passphrase. Keystore migration "
                   << "already performed.";
      return;
    }
    // Will fail if we already have an explicit passphrase or we have pending
    // keys.
    SetCustomPassphrase(passphrase, &trans, &node);
    return;
  }

  std::string bootstrap_token;
  sync_pb::EncryptedData pending_keys;
  if (cryptographer->has_pending_keys())
    pending_keys = cryptographer->GetPendingKeys();
  bool success = false;

  // There are six cases to handle here:
  // 1. The user has no pending keys and is setting their current GAIA password
  //    as the encryption passphrase. This happens either during first time sync
  //    with a clean profile, or after re-authenticating on a profile that was
  //    already signed in with the cryptographer ready.
  // 2. The user has no pending keys, and is overwriting an (already provided)
  //    implicit passphrase with an explicit (custom) passphrase.
  // 3. The user has pending keys for an explicit passphrase that is somehow set
  //    to their current GAIA passphrase.
  // 4. The user has pending keys encrypted with their current GAIA passphrase
  //    and the caller passes in the current GAIA passphrase.
  // 5. The user has pending keys encrypted with an older GAIA passphrase
  //    and the caller passes in the current GAIA passphrase.
  // 6. The user has previously done encryption with an explicit passphrase.
  // Furthermore, we enforce the fact that the bootstrap encryption token will
  // always be derived from the newest GAIA password if the account is using
  // an implicit passphrase (even if the data is encrypted with an old GAIA
  // password). If the account is using an explicit (custom) passphrase, the
  // bootstrap token will be derived from the most recently provided explicit
  // passphrase (that was able to decrypt the data).
  if (!IsExplicitPassphrase(passphrase_type_)) {
    if (!cryptographer->has_pending_keys()) {
      if (cryptographer->AddKey(key_params)) {
        // Case 1 and 2. We set a new GAIA passphrase when there are no pending
        // keys (1), or overwriting an implicit passphrase with a new explicit
        // one (2) when there are no pending keys.
        if (is_explicit) {
          DVLOG(1) << "Setting explicit passphrase for encryption.";
          passphrase_type_ = CUSTOM_PASSPHRASE;
          FOR_EACH_OBSERVER(SyncEncryptionHandler::Observer, observers_,
                            OnPassphraseTypeChanged(passphrase_type_));
        } else {
          DVLOG(1) << "Setting implicit passphrase for encryption.";
        }
        cryptographer->GetBootstrapToken(&bootstrap_token);
        success = true;
      } else {
        NOTREACHED() << "Failed to add key to cryptographer.";
        success = false;
      }
    } else {  // cryptographer->has_pending_keys() == true
      if (is_explicit) {
        // This can only happen if the nigori node is updated with a new
        // implicit passphrase while a client is attempting to set a new custom
        // passphrase (race condition).
        DVLOG(1) << "Failing because an implicit passphrase is already set.";
        success = false;
      } else {  // is_explicit == false
        if (cryptographer->DecryptPendingKeys(key_params)) {
          // Case 4. We successfully decrypted with the implicit GAIA passphrase
          // passed in.
          DVLOG(1) << "Implicit internal passphrase accepted for decryption.";
          cryptographer->GetBootstrapToken(&bootstrap_token);
          success = true;
        } else {
          // Case 5. Encryption was done with an old GAIA password, but we were
          // provided with the current GAIA password. We need to generate a new
          // bootstrap token to preserve it. We build a temporary cryptographer
          // to allow us to extract these params without polluting our current
          // cryptographer.
          DVLOG(1) << "Implicit internal passphrase failed to decrypt, adding "
                   << "anyways as default passphrase and persisting via "
                   << "bootstrap token.";
          Cryptographer temp_cryptographer(cryptographer->encryptor());
          temp_cryptographer.AddKey(key_params);
          temp_cryptographer.GetBootstrapToken(&bootstrap_token);
          // We then set the new passphrase as the default passphrase of the
          // real cryptographer, even though we have pending keys. This is safe,
          // as although Cryptographer::is_initialized() will now be true,
          // is_ready() will remain false due to having pending keys.
          cryptographer->AddKey(key_params);
          success = false;
        }
      }  // is_explicit
    }  // cryptographer->has_pending_keys()
  } else {  // IsExplicitPassphrase(passphrase_type_) == true.
    // Case 6. We do not want to override a previously set explicit passphrase,
    // so we return a failure.
    DVLOG(1) << "Failing because an explicit passphrase is already set.";
    success = false;
  }

  DVLOG_IF(1, !success)
      << "Failure in SetEncryptionPassphrase; notifying and returning.";
  DVLOG_IF(1, success)
      << "Successfully set encryption passphrase; updating nigori and "
         "reencrypting.";

  FinishSetPassphrase(success, bootstrap_token, &trans, &node);
}

void SyncEncryptionHandlerImpl::SetDecryptionPassphrase(
    const std::string& passphrase) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // We do not accept empty passphrases.
  if (passphrase.empty()) {
    NOTREACHED() << "Cannot decrypt with an empty passphrase.";
    return;
  }

  // All accesses to the cryptographer are protected by a transaction.
  WriteTransaction trans(FROM_HERE, user_share_);
  KeyParams key_params = {"localhost", "dummy", passphrase};
  WriteNode node(&trans);
  if (node.InitByTagLookup(kNigoriTag) != BaseNode::INIT_OK) {
    NOTREACHED();
    return;
  }

  // Once we've migrated to keystore, we're only ever decrypting keys derived
  // from an explicit passphrase. But, for clients without a keystore key yet
  // (either not on by default or failed to download one), we still support
  // decrypting with a gaia passphrase, and therefore bypass the
  // DecryptPendingKeysWithExplicitPassphrase logic.
  if (IsNigoriMigratedToKeystore(node.GetNigoriSpecifics()) &&
      IsExplicitPassphrase(passphrase_type_)) {
    DecryptPendingKeysWithExplicitPassphrase(passphrase, &trans, &node);
    return;
  }

  Cryptographer* cryptographer =
      &UnlockVaultMutable(trans.GetWrappedTrans())->cryptographer;
  if (!cryptographer->has_pending_keys()) {
    // Note that this *can* happen in a rare situation where data is
    // re-encrypted on another client while a SetDecryptionPassphrase() call is
    // in-flight on this client. It is rare enough that we choose to do nothing.
    NOTREACHED() << "Attempt to set decryption passphrase failed because there "
                 << "were no pending keys.";
    return;
  }

  std::string bootstrap_token;
  sync_pb::EncryptedData pending_keys;
  pending_keys = cryptographer->GetPendingKeys();
  bool success = false;

  // There are three cases to handle here:
  // 7. We're using the current GAIA password to decrypt the pending keys. This
  //    happens when signing in to an account with a previously set implicit
  //    passphrase, where the data is already encrypted with the newest GAIA
  //    password.
  // 8. The user is providing an old GAIA password to decrypt the pending keys.
  //    In this case, the user is using an implicit passphrase, but has changed
  //    their password since they last encrypted their data, and therefore
  //    their current GAIA password was unable to decrypt the data. This will
  //    happen when the user is setting up a new profile with a previously
  //    encrypted account (after changing passwords).
  // 9. The user is providing a previously set explicit passphrase to decrypt
  //    the pending keys.
  if (!IsExplicitPassphrase(passphrase_type_)) {
    if (cryptographer->is_initialized()) {
      // We only want to change the default encryption key to the pending
      // one if the pending keybag already contains the current default.
      // This covers the case where a different client re-encrypted
      // everything with a newer gaia passphrase (and hence the keybag
      // contains keys from all previously used gaia passphrases).
      // Otherwise, we're in a situation where the pending keys are
      // encrypted with an old gaia passphrase, while the default is the
      // current gaia passphrase. In that case, we preserve the default.
      Cryptographer temp_cryptographer(cryptographer->encryptor());
      temp_cryptographer.SetPendingKeys(cryptographer->GetPendingKeys());
      if (temp_cryptographer.DecryptPendingKeys(key_params)) {
        // Check to see if the pending bag of keys contains the current
        // default key.
        sync_pb::EncryptedData encrypted;
        cryptographer->GetKeys(&encrypted);
        if (temp_cryptographer.CanDecrypt(encrypted)) {
          DVLOG(1) << "Implicit user provided passphrase accepted for "
                   << "decryption, overwriting default.";
          // Case 7. The pending keybag contains the current default. Go ahead
          // and update the cryptographer, letting the default change.
          cryptographer->DecryptPendingKeys(key_params);
          cryptographer->GetBootstrapToken(&bootstrap_token);
          success = true;
        } else {
          // Case 8. The pending keybag does not contain the current default
          // encryption key. We decrypt the pending keys here, and in
          // FinishSetPassphrase, re-encrypt everything with the current GAIA
          // passphrase instead of the passphrase just provided by the user.
          DVLOG(1) << "Implicit user provided passphrase accepted for "
                   << "decryption, restoring implicit internal passphrase "
                   << "as default.";
          std::string bootstrap_token_from_current_key;
          cryptographer->GetBootstrapToken(
              &bootstrap_token_from_current_key);
          cryptographer->DecryptPendingKeys(key_params);
          // Overwrite the default from the pending keys.
          cryptographer->AddKeyFromBootstrapToken(
              bootstrap_token_from_current_key);
          success = true;
        }
      } else {  // !temp_cryptographer.DecryptPendingKeys(..)
        DVLOG(1) << "Implicit user provided passphrase failed to decrypt.";
        success = false;
      }  // temp_cryptographer.DecryptPendingKeys(...)
    } else {  // cryptographer->is_initialized() == false
      if (cryptographer->DecryptPendingKeys(key_params)) {
        // This can happpen in two cases:
        // - First time sync on android, where we'll never have a
        //   !user_provided passphrase.
        // - This is a restart for a client that lost their bootstrap token.
        // In both cases, we should go ahead and initialize the cryptographer
        // and persist the new bootstrap token.
        //
        // Note: at this point, we cannot distinguish between cases 7 and 8
        // above. This user provided passphrase could be the current or the
        // old. But, as long as we persist the token, there's nothing more
        // we can do.
        cryptographer->GetBootstrapToken(&bootstrap_token);
        DVLOG(1) << "Implicit user provided passphrase accepted, initializing"
                 << " cryptographer.";
        success = true;
      } else {
        DVLOG(1) << "Implicit user provided passphrase failed to decrypt.";
        success = false;
      }
    }  // cryptographer->is_initialized()
  } else {  // nigori_has_explicit_passphrase == true
    // Case 9. Encryption was done with an explicit passphrase, and we decrypt
    // with the passphrase provided by the user.
    if (cryptographer->DecryptPendingKeys(key_params)) {
      DVLOG(1) << "Explicit passphrase accepted for decryption.";
      cryptographer->GetBootstrapToken(&bootstrap_token);
      success = true;
    } else {
      DVLOG(1) << "Explicit passphrase failed to decrypt.";
      success = false;
    }
  }  // nigori_has_explicit_passphrase

  DVLOG_IF(1, !success)
      << "Failure in SetDecryptionPassphrase; notifying and returning.";
  DVLOG_IF(1, success)
      << "Successfully set decryption passphrase; updating nigori and "
         "reencrypting.";

  FinishSetPassphrase(success, bootstrap_token, &trans, &node);
}

void SyncEncryptionHandlerImpl::EnableEncryptEverything() {
  DCHECK(thread_checker_.CalledOnValidThread());
  WriteTransaction trans(FROM_HERE, user_share_);
  DVLOG(1) << "Enabling encrypt everything.";
  if (encrypt_everything_)
    return;
  EnableEncryptEverythingImpl(trans.GetWrappedTrans());
  WriteEncryptionStateToNigori(&trans);
  if (UnlockVault(trans.GetWrappedTrans()).cryptographer.is_ready())
    ReEncryptEverything(&trans);
}

bool SyncEncryptionHandlerImpl::EncryptEverythingEnabled() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return encrypt_everything_;
}

PassphraseType SyncEncryptionHandlerImpl::GetPassphraseType() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return passphrase_type_;
}

// Note: this is called from within a syncable transaction, so we need to post
// tasks if we want to do any work that creates a new sync_api transaction.
void SyncEncryptionHandlerImpl::ApplyNigoriUpdate(
    const sync_pb::NigoriSpecifics& nigori,
    syncable::BaseTransaction* const trans) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(trans);
  if (!ApplyNigoriUpdateImpl(nigori, trans)) {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&SyncEncryptionHandlerImpl::RewriteNigori,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  FOR_EACH_OBSERVER(
      SyncEncryptionHandler::Observer,
      observers_,
      OnCryptographerStateChanged(
          &UnlockVaultMutable(trans)->cryptographer));
}

void SyncEncryptionHandlerImpl::UpdateNigoriFromEncryptedTypes(
    sync_pb::NigoriSpecifics* nigori,
    syncable::BaseTransaction* const trans) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  syncable::UpdateNigoriFromEncryptedTypes(UnlockVault(trans).encrypted_types,
                                           encrypt_everything_,
                                           nigori);
}

bool SyncEncryptionHandlerImpl::NeedKeystoreKey(
    syncable::BaseTransaction* const trans) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return keystore_key_.empty();
}

bool SyncEncryptionHandlerImpl::SetKeystoreKey(
    const std::string& key,
    syncable::BaseTransaction* const trans) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!keystore_key_.empty() || key.empty())
    return false;
  keystore_key_ = key;

  DVLOG(1) << "Keystore bootstrap token updated.";
  FOR_EACH_OBSERVER(SyncEncryptionHandler::Observer, observers_,
                    OnBootstrapTokenUpdated(key,
                                            KEYSTORE_BOOTSTRAP_TOKEN));

  Cryptographer* cryptographer = &UnlockVaultMutable(trans)->cryptographer;
  syncable::Entry entry(trans, syncable::GET_BY_SERVER_TAG, kNigoriTag);
  if (entry.good()) {
    const sync_pb::NigoriSpecifics& nigori =
        entry.Get(syncable::SPECIFICS).nigori();
    if (cryptographer->has_pending_keys() &&
        IsNigoriMigratedToKeystore(nigori) &&
        !nigori.keystore_decryptor_token().blob().empty()) {
      // If the nigori is already migrated and we have pending keys, we might
      // be able to decrypt them using the keystore decryptor token.
      DecryptPendingKeysWithKeystoreKey(keystore_key_,
                                        nigori.keystore_decryptor_token(),
                                        cryptographer);
    } else if (ShouldTriggerMigration(nigori, *cryptographer)) {
      // We call rewrite nigori to attempt to trigger migration.
      // Need to post a task to open a new sync_api transaction.
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&SyncEncryptionHandlerImpl::RewriteNigori,
                     weak_ptr_factory_.GetWeakPtr()));
    }
  }

  return true;
}

ModelTypeSet SyncEncryptionHandlerImpl::GetEncryptedTypes(
    syncable::BaseTransaction* const trans) const {
  return UnlockVault(trans).encrypted_types;
}

Cryptographer* SyncEncryptionHandlerImpl::GetCryptographerUnsafe() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return &vault_unsafe_.cryptographer;
}

ModelTypeSet SyncEncryptionHandlerImpl::GetEncryptedTypesUnsafe() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return vault_unsafe_.encrypted_types;
}

bool SyncEncryptionHandlerImpl::MigratedToKeystore() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ReadTransaction trans(FROM_HERE, user_share_);
  ReadNode nigori_node(&trans);
  if (nigori_node.InitByTagLookup(kNigoriTag) != BaseNode::INIT_OK)
    return false;
  return IsNigoriMigratedToKeystore(nigori_node.GetNigoriSpecifics());
}

// This function iterates over all encrypted types.  There are many scenarios in
// which data for some or all types is not currently available.  In that case,
// the lookup of the root node will fail and we will skip encryption for that
// type.
void SyncEncryptionHandlerImpl::ReEncryptEverything(
    WriteTransaction* trans) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(UnlockVault(trans->GetWrappedTrans()).cryptographer.is_ready());
  for (ModelTypeSet::Iterator iter =
           UnlockVault(trans->GetWrappedTrans()).encrypted_types.First();
       iter.Good(); iter.Inc()) {
    if (iter.Get() == PASSWORDS || IsControlType(iter.Get()))
      continue; // These types handle encryption differently.

    ReadNode type_root(trans);
    std::string tag = ModelTypeToRootTag(iter.Get());
    if (type_root.InitByTagLookup(tag) != BaseNode::INIT_OK)
      continue; // Don't try to reencrypt if the type's data is unavailable.

    // Iterate through all children of this datatype.
    std::queue<int64> to_visit;
    int64 child_id = type_root.GetFirstChildId();
    to_visit.push(child_id);
    while (!to_visit.empty()) {
      child_id = to_visit.front();
      to_visit.pop();
      if (child_id == kInvalidId)
        continue;

      WriteNode child(trans);
      if (child.InitByIdLookup(child_id) != BaseNode::INIT_OK) {
        NOTREACHED();
        continue;
      }
      if (child.GetIsFolder()) {
        to_visit.push(child.GetFirstChildId());
      }
      if (child.GetEntry()->Get(syncable::UNIQUE_SERVER_TAG).empty()) {
      // Rewrite the specifics of the node with encrypted data if necessary
      // (only rewrite the non-unique folders).
        child.ResetFromSpecifics();
      }
      to_visit.push(child.GetSuccessorId());
    }
  }

  // Passwords are encrypted with their own legacy scheme.  Passwords are always
  // encrypted so we don't need to check GetEncryptedTypes() here.
  ReadNode passwords_root(trans);
  std::string passwords_tag = ModelTypeToRootTag(PASSWORDS);
  if (passwords_root.InitByTagLookup(passwords_tag) ==
          BaseNode::INIT_OK) {
    int64 child_id = passwords_root.GetFirstChildId();
    while (child_id != kInvalidId) {
      WriteNode child(trans);
      if (child.InitByIdLookup(child_id) != BaseNode::INIT_OK) {
        NOTREACHED();
        return;
      }
      child.SetPasswordSpecifics(child.GetPasswordSpecifics());
      child_id = child.GetSuccessorId();
    }
  }

  DVLOG(1) << "Re-encrypt everything complete.";

  // NOTE: We notify from within a transaction.
  FOR_EACH_OBSERVER(SyncEncryptionHandler::Observer, observers_,
                    OnEncryptionComplete());
}

bool SyncEncryptionHandlerImpl::ApplyNigoriUpdateImpl(
    const sync_pb::NigoriSpecifics& nigori,
    syncable::BaseTransaction* const trans) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "Applying nigori node update.";
  bool nigori_types_need_update = !UpdateEncryptedTypesFromNigori(nigori,
                                                                  trans);
  bool is_nigori_migrated = IsNigoriMigratedToKeystore(nigori);
  if (is_nigori_migrated) {
    migration_time_ms_ = nigori.keystore_migration_time();
    PassphraseType nigori_passphrase_type =
        ProtoPassphraseTypeToEnum(nigori.passphrase_type());

    // Only update the local passphrase state if it's a valid transition:
    // - implicit -> keystore
    // - implicit -> frozen implicit
    // - implicit -> custom
    // - keystore -> custom
    // Note: frozen implicit -> custom is not technically a valid transition,
    // but we let it through here as well in case future versions do add support
    // for this transition.
    if (passphrase_type_ != nigori_passphrase_type &&
        nigori_passphrase_type != IMPLICIT_PASSPHRASE &&
        (passphrase_type_ == IMPLICIT_PASSPHRASE ||
         nigori_passphrase_type == CUSTOM_PASSPHRASE)) {
      DVLOG(1) << "Changing passphrase state from "
               << PassphraseTypeToString(passphrase_type_)
               << " to "
               << PassphraseTypeToString(nigori_passphrase_type);
      passphrase_type_ = nigori_passphrase_type;
      FOR_EACH_OBSERVER(SyncEncryptionHandler::Observer, observers_,
                        OnPassphraseTypeChanged(passphrase_type_));
    }
    if (passphrase_type_ == KEYSTORE_PASSPHRASE && encrypt_everything_) {
      // This is the case where another client that didn't support keystore
      // encryption attempted to enable full encryption. We detect it
      // and switch the passphrase type to frozen implicit passphrase instead
      // due to full encryption not being compatible with keystore passphrase.
      // Because the local passphrase type will not match the nigori passphrase
      // type, we will trigger a rewrite and subsequently a re-migration.
      DVLOG(1) << "Changing passphrase state to FROZEN_IMPLICIT_PASSPHRASE "
               << "due to full encryption.";
      passphrase_type_ = FROZEN_IMPLICIT_PASSPHRASE;
      FOR_EACH_OBSERVER(SyncEncryptionHandler::Observer, observers_,
                        OnPassphraseTypeChanged(passphrase_type_));
    }
  } else {
    // It's possible that while we're waiting for migration a client that does
    // not have keystore encryption enabled switches to a custom passphrase.
    if (nigori.keybag_is_frozen() &&
        passphrase_type_ != CUSTOM_PASSPHRASE) {
      passphrase_type_ = CUSTOM_PASSPHRASE;
      FOR_EACH_OBSERVER(SyncEncryptionHandler::Observer, observers_,
                        OnPassphraseTypeChanged(passphrase_type_));
    }
  }

  Cryptographer* cryptographer = &UnlockVaultMutable(trans)->cryptographer;
  bool nigori_needs_new_keys = false;
  if (!nigori.encryption_keybag().blob().empty()) {
    // We only update the default key if this was a new explicit passphrase.
    // Else, since it was decryptable, it must not have been a new key.
    bool need_new_default_key = false;
    if (is_nigori_migrated) {
      need_new_default_key = IsExplicitPassphrase(
          ProtoPassphraseTypeToEnum(nigori.passphrase_type()));
    } else {
      need_new_default_key = nigori.keybag_is_frozen();
    }
    if (!AttemptToInstallKeybag(nigori.encryption_keybag(),
                                need_new_default_key,
                                cryptographer)) {
      // Check to see if we can decrypt the keybag using the keystore decryptor
      // token.
      cryptographer->SetPendingKeys(nigori.encryption_keybag());
      if (!nigori.keystore_decryptor_token().blob().empty() &&
          !keystore_key_.empty()) {
        if (DecryptPendingKeysWithKeystoreKey(keystore_key_,
                                              nigori.keystore_decryptor_token(),
                                              cryptographer)) {
          nigori_needs_new_keys =
              cryptographer->KeybagIsStale(nigori.encryption_keybag());
        } else {
          LOG(ERROR) << "Failed to decrypt pending keys using keystore "
                     << "bootstrap key.";
        }
      }
    } else {
      // Keybag was installed. We write back our local keybag into the nigori
      // node if the nigori node's keybag either contains less keys or
      // has a different default key.
      nigori_needs_new_keys =
          cryptographer->KeybagIsStale(nigori.encryption_keybag());
    }
  } else {
    // The nigori node has an empty encryption keybag. Attempt to write our
    // local encryption keys into it.
    LOG(WARNING) << "Nigori had empty encryption keybag.";
    nigori_needs_new_keys = true;
  }

  // If we've completed a sync cycle and the cryptographer isn't ready
  // yet or has pending keys, prompt the user for a passphrase.
  if (cryptographer->has_pending_keys()) {
    DVLOG(1) << "OnPassphraseRequired Sent";
    sync_pb::EncryptedData pending_keys = cryptographer->GetPendingKeys();
    FOR_EACH_OBSERVER(SyncEncryptionHandler::Observer, observers_,
                      OnPassphraseRequired(REASON_DECRYPTION,
                                           pending_keys));
  } else if (!cryptographer->is_ready()) {
    DVLOG(1) << "OnPassphraseRequired sent because cryptographer is not "
             << "ready";
    FOR_EACH_OBSERVER(SyncEncryptionHandler::Observer, observers_,
                      OnPassphraseRequired(REASON_ENCRYPTION,
                                           sync_pb::EncryptedData()));
  }

  // Check if the current local encryption state is stricter/newer than the
  // nigori state. If so, we need to overwrite the nigori node with the local
  // state.
  bool passphrase_type_matches = true;
  if (!is_nigori_migrated) {
    DCHECK(passphrase_type_ == CUSTOM_PASSPHRASE ||
           passphrase_type_ == IMPLICIT_PASSPHRASE);
    passphrase_type_matches =
        nigori.keybag_is_frozen() == IsExplicitPassphrase(passphrase_type_);
  } else {
    passphrase_type_matches =
        (ProtoPassphraseTypeToEnum(nigori.passphrase_type()) ==
             passphrase_type_);
  }
  if (!passphrase_type_matches ||
      nigori.encrypt_everything() != encrypt_everything_ ||
      nigori_types_need_update ||
      nigori_needs_new_keys) {
    DVLOG(1) << "Triggering nigori rewrite.";
    return false;
  }
  return true;
}

void SyncEncryptionHandlerImpl::RewriteNigori() {
  DVLOG(1) << "Writing local encryption state into nigori.";
  DCHECK(thread_checker_.CalledOnValidThread());
  WriteTransaction trans(FROM_HERE, user_share_);
  WriteEncryptionStateToNigori(&trans);
}

void SyncEncryptionHandlerImpl::WriteEncryptionStateToNigori(
    WriteTransaction* trans) {
  DCHECK(thread_checker_.CalledOnValidThread());
  WriteNode nigori_node(trans);
  // This can happen in tests that don't have nigori nodes.
  if (nigori_node.InitByTagLookup(kNigoriTag) != BaseNode::INIT_OK)
    return;

  sync_pb::NigoriSpecifics nigori = nigori_node.GetNigoriSpecifics();
  const Cryptographer& cryptographer =
      UnlockVault(trans->GetWrappedTrans()).cryptographer;

  // Will not do anything if we shouldn't or can't migrate. Otherwise
  // migrates, writing the full encryption state as it does.
  if (!AttemptToMigrateNigoriToKeystore(trans, &nigori_node)) {
    if (cryptographer.is_ready() &&
        nigori_overwrite_count_ < kNigoriOverwriteLimit) {
      // Does not modify the encrypted blob if the unencrypted data already
      // matches what is about to be written.
      sync_pb::EncryptedData original_keys = nigori.encryption_keybag();
      if (!cryptographer.GetKeys(nigori.mutable_encryption_keybag()))
        NOTREACHED();

      if (nigori.encryption_keybag().SerializeAsString() !=
          original_keys.SerializeAsString()) {
        // We've updated the nigori node's encryption keys. In order to prevent
        // a possible looping of two clients constantly overwriting each other,
        // we limit the absolute number of overwrites per client instantiation.
        nigori_overwrite_count_++;
        UMA_HISTOGRAM_COUNTS("Sync.AutoNigoriOverwrites",
                             nigori_overwrite_count_);
      }

      // Note: we don't try to set keybag_is_frozen here since if that
      // is lost the user can always set it again (and we don't want to clobber
      // any migration state). The main goal at this point is to preserve
      // the encryption keys so all data remains decryptable.
    }
    syncable::UpdateNigoriFromEncryptedTypes(
        UnlockVault(trans->GetWrappedTrans()).encrypted_types,
        encrypt_everything_,
        &nigori);

    // If nothing has changed, this is a no-op.
    nigori_node.SetNigoriSpecifics(nigori);
  }
}

bool SyncEncryptionHandlerImpl::UpdateEncryptedTypesFromNigori(
    const sync_pb::NigoriSpecifics& nigori,
    syncable::BaseTransaction* const trans) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ModelTypeSet* encrypted_types = &UnlockVaultMutable(trans)->encrypted_types;
  if (nigori.encrypt_everything()) {
    EnableEncryptEverythingImpl(trans);
    DCHECK(encrypted_types->Equals(UserTypes()));
    return true;
  } else if (encrypt_everything_) {
    DCHECK(encrypted_types->Equals(UserTypes()));
    return false;
  }

  ModelTypeSet nigori_encrypted_types;
  nigori_encrypted_types = syncable::GetEncryptedTypesFromNigori(nigori);
  nigori_encrypted_types.PutAll(SensitiveTypes());

  // If anything more than the sensitive types were encrypted, and
  // encrypt_everything is not explicitly set to false, we assume it means
  // a client intended to enable encrypt everything.
  if (!nigori.has_encrypt_everything() &&
      !Difference(nigori_encrypted_types, SensitiveTypes()).Empty()) {
    if (!encrypt_everything_) {
      encrypt_everything_ = true;
      *encrypted_types = UserTypes();
      FOR_EACH_OBSERVER(
          Observer, observers_,
          OnEncryptedTypesChanged(*encrypted_types, encrypt_everything_));
    }
    DCHECK(encrypted_types->Equals(UserTypes()));
    return false;
  }

  MergeEncryptedTypes(nigori_encrypted_types, trans);
  return encrypted_types->Equals(nigori_encrypted_types);
}

void SyncEncryptionHandlerImpl::SetCustomPassphrase(
    const std::string& passphrase,
    WriteTransaction* trans,
    WriteNode* nigori_node) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsNigoriMigratedToKeystore(nigori_node->GetNigoriSpecifics()));
  KeyParams key_params = {"localhost", "dummy", passphrase};

  if (passphrase_type_ != KEYSTORE_PASSPHRASE) {
    DVLOG(1) << "Failing to set a custom passphrase because one has already "
             << "been set.";
    FinishSetPassphrase(false, "", trans, nigori_node);
    return;
  }

  Cryptographer* cryptographer =
      &UnlockVaultMutable(trans->GetWrappedTrans())->cryptographer;
  if (cryptographer->has_pending_keys()) {
    // This theoretically shouldn't happen, because the only way to have pending
    // keys after migrating to keystore support is if a custom passphrase was
    // set, which should update passpshrase_state_ and should be caught by the
    // if statement above. For the sake of safety though, we check for it in
    // case a client is misbehaving.
    LOG(ERROR) << "Failing to set custom passphrase because of pending keys.";
    FinishSetPassphrase(false, "", trans, nigori_node);
    return;
  }

  std::string bootstrap_token;
  if (cryptographer->AddKey(key_params)) {
    DVLOG(1) << "Setting custom passphrase.";
    cryptographer->GetBootstrapToken(&bootstrap_token);
    passphrase_type_ = CUSTOM_PASSPHRASE;
    FOR_EACH_OBSERVER(SyncEncryptionHandler::Observer, observers_,
                      OnPassphraseTypeChanged(passphrase_type_));
  } else {
    NOTREACHED() << "Failed to add key to cryptographer.";
    return;
  }
  FinishSetPassphrase(true, bootstrap_token, trans, nigori_node);
}

void SyncEncryptionHandlerImpl::DecryptPendingKeysWithExplicitPassphrase(
    const std::string& passphrase,
    WriteTransaction* trans,
    WriteNode* nigori_node) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsExplicitPassphrase(passphrase_type_));
  KeyParams key_params = {"localhost", "dummy", passphrase};

  Cryptographer* cryptographer =
      &UnlockVaultMutable(trans->GetWrappedTrans())->cryptographer;
  if (!cryptographer->has_pending_keys()) {
    // Note that this *can* happen in a rare situation where data is
    // re-encrypted on another client while a SetDecryptionPassphrase() call is
    // in-flight on this client. It is rare enough that we choose to do nothing.
    NOTREACHED() << "Attempt to set decryption passphrase failed because there "
                 << "were no pending keys.";
    return;
  }

  DCHECK(IsExplicitPassphrase(passphrase_type_));
  bool success = false;
  std::string bootstrap_token;
  if (cryptographer->DecryptPendingKeys(key_params)) {
    DVLOG(1) << "Explicit passphrase accepted for decryption.";
    cryptographer->GetBootstrapToken(&bootstrap_token);
    success = true;
  } else {
    DVLOG(1) << "Explicit passphrase failed to decrypt.";
    success = false;
  }
  if (success && !keystore_key_.empty()) {
    // Should already be part of the encryption keybag, but we add it just
    // in case.
    KeyParams key_params = {"localhost", "dummy", keystore_key_};
    cryptographer->AddNonDefaultKey(key_params);
  }
  FinishSetPassphrase(success, bootstrap_token, trans, nigori_node);
}

void SyncEncryptionHandlerImpl::FinishSetPassphrase(
    bool success,
    const std::string& bootstrap_token,
    WriteTransaction* trans,
    WriteNode* nigori_node) {
  DCHECK(thread_checker_.CalledOnValidThread());
  FOR_EACH_OBSERVER(
      SyncEncryptionHandler::Observer,
      observers_,
      OnCryptographerStateChanged(
          &UnlockVaultMutable(trans->GetWrappedTrans())->cryptographer));

  // It's possible we need to change the bootstrap token even if we failed to
  // set the passphrase (for example if we need to preserve the new GAIA
  // passphrase).
  if (!bootstrap_token.empty()) {
    DVLOG(1) << "Passphrase bootstrap token updated.";
    FOR_EACH_OBSERVER(SyncEncryptionHandler::Observer, observers_,
                      OnBootstrapTokenUpdated(bootstrap_token,
                                              PASSPHRASE_BOOTSTRAP_TOKEN));
  }

  const Cryptographer& cryptographer =
      UnlockVault(trans->GetWrappedTrans()).cryptographer;
  if (!success) {
    if (cryptographer.is_ready()) {
      LOG(ERROR) << "Attempt to change passphrase failed while cryptographer "
                 << "was ready.";
    } else if (cryptographer.has_pending_keys()) {
      FOR_EACH_OBSERVER(SyncEncryptionHandler::Observer, observers_,
                        OnPassphraseRequired(REASON_DECRYPTION,
                                             cryptographer.GetPendingKeys()));
    } else {
      FOR_EACH_OBSERVER(SyncEncryptionHandler::Observer, observers_,
                        OnPassphraseRequired(REASON_ENCRYPTION,
                                             sync_pb::EncryptedData()));
    }
    return;
  }
  DCHECK(success);
  DCHECK(cryptographer.is_ready());

  // Will do nothing if we're already properly migrated or unable to migrate
  // (in otherwords, if ShouldTriggerMigration is false).
  // Otherwise will update the nigori node with the current migrated state,
  // writing all encryption state as it does.
  if (!AttemptToMigrateNigoriToKeystore(trans, nigori_node)) {
    sync_pb::NigoriSpecifics nigori(nigori_node->GetNigoriSpecifics());
    // Does not modify nigori.encryption_keybag() if the original decrypted
    // data was the same.
    if (!cryptographer.GetKeys(nigori.mutable_encryption_keybag()))
      NOTREACHED();
    if (IsNigoriMigratedToKeystore(nigori)) {
      DCHECK(keystore_key_.empty() || IsExplicitPassphrase(passphrase_type_));
      DVLOG(1) << "Leaving nigori migration state untouched after setting"
               << " passphrase.";
    } else {
      nigori.set_keybag_is_frozen(
          IsExplicitPassphrase(passphrase_type_));
    }
    nigori_node->SetNigoriSpecifics(nigori);
  }

  // Must do this after OnPassphraseTypeChanged, in order to ensure the PSS
  // checks the passphrase state after it has been set.
  FOR_EACH_OBSERVER(SyncEncryptionHandler::Observer, observers_,
                    OnPassphraseAccepted());

  // Does nothing if everything is already encrypted.
  // TODO(zea): If we just migrated and enabled encryption, this will be
  // redundant. Figure out a way to not do this unnecessarily.
  ReEncryptEverything(trans);
}

void SyncEncryptionHandlerImpl::MergeEncryptedTypes(
    ModelTypeSet new_encrypted_types,
    syncable::BaseTransaction* const trans) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Only UserTypes may be encrypted.
  DCHECK(UserTypes().HasAll(new_encrypted_types));

  ModelTypeSet* encrypted_types = &UnlockVaultMutable(trans)->encrypted_types;
  if (!encrypted_types->HasAll(new_encrypted_types)) {
    *encrypted_types = new_encrypted_types;
    FOR_EACH_OBSERVER(
        Observer, observers_,
        OnEncryptedTypesChanged(*encrypted_types, encrypt_everything_));
  }
}

SyncEncryptionHandlerImpl::Vault* SyncEncryptionHandlerImpl::UnlockVaultMutable(
    syncable::BaseTransaction* const trans) {
  DCHECK_EQ(user_share_->directory.get(), trans->directory());
  return &vault_unsafe_;
}

const SyncEncryptionHandlerImpl::Vault& SyncEncryptionHandlerImpl::UnlockVault(
    syncable::BaseTransaction* const trans) const {
  DCHECK_EQ(user_share_->directory.get(), trans->directory());
  return vault_unsafe_;
}

bool SyncEncryptionHandlerImpl::ShouldTriggerMigration(
    const sync_pb::NigoriSpecifics& nigori,
    const Cryptographer& cryptographer) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  // TODO(zea): once we're willing to have the keystore key be the only
  // encryption key, change this to !has_pending_keys(). For now, we need the
  // cryptographer to be initialized with the current GAIA pass so that older
  // clients (that don't have keystore support) can decrypt the keybag.
  if (!cryptographer.is_ready())
    return false;
  if (IsNigoriMigratedToKeystore(nigori)) {
    // If the nigori is already migrated but does not reflect the explicit
    // passphrase state, remigrate. Similarly, if the nigori has an explicit
    // passphrase but does not have full encryption, or the nigori has an
    // implicit passphrase but does have full encryption, re-migrate.
    // Note that this is to defend against other clients without keystore
    // encryption enabled transitioning to states that are no longer valid.
    if (passphrase_type_ != KEYSTORE_PASSPHRASE &&
        nigori.passphrase_type() ==
            sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE) {
      return true;
    } else if (IsExplicitPassphrase(passphrase_type_) &&
               !encrypt_everything_) {
      return true;
    } else if (passphrase_type_ == KEYSTORE_PASSPHRASE &&
               encrypt_everything_) {
      return true;
    } else {
      return false;
    }
  } else if (keystore_key_.empty()) {
    // If we haven't already migrated, we don't want to do anything unless
    // a keystore key is available (so that those clients without keystore
    // encryption enabled aren't forced into new states, e.g. frozen implicit
    // passphrase).
    return false;
  }
  return true;
}

bool SyncEncryptionHandlerImpl::AttemptToMigrateNigoriToKeystore(
    WriteTransaction* trans,
    WriteNode* nigori_node) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const sync_pb::NigoriSpecifics& old_nigori =
      nigori_node->GetNigoriSpecifics();
  Cryptographer* cryptographer =
      &UnlockVaultMutable(trans->GetWrappedTrans())->cryptographer;

  if (!ShouldTriggerMigration(old_nigori, *cryptographer))
    return false;

  DVLOG(1) << "Starting nigori migration to keystore support.";
  if (migration_time_ms_ == 0)
    migration_time_ms_ = TimeToProtoTime(base::Time::Now());
  sync_pb::NigoriSpecifics migrated_nigori(old_nigori);
  migrated_nigori.set_keystore_migration_time(migration_time_ms_);

  PassphraseType new_passphrase_type = passphrase_type_;
  bool new_encrypt_everything = encrypt_everything_;
  if (encrypt_everything_ && !IsExplicitPassphrase(passphrase_type_)) {
    DVLOG(1) << "Switching to frozen implicit passphrase due to already having "
             << "full encryption.";
    new_passphrase_type = FROZEN_IMPLICIT_PASSPHRASE;
    migrated_nigori.clear_keystore_decryptor_token();
  } else if (IsExplicitPassphrase(passphrase_type_)) {
    DVLOG_IF(1, !encrypt_everything_) << "Enabling encrypt everything due to "
                                      << "explicit passphrase";
    new_encrypt_everything = true;
    migrated_nigori.clear_keystore_decryptor_token();
  } else {
    DCHECK_EQ(passphrase_type_, IMPLICIT_PASSPHRASE);
    DCHECK(!encrypt_everything_);
    new_passphrase_type = KEYSTORE_PASSPHRASE;
    DVLOG(1) << "Switching to keystore passphrase state.";
  }
  migrated_nigori.set_encrypt_everything(new_encrypt_everything);
  migrated_nigori.set_passphrase_type(
      EnumPassphraseTypeToProto(new_passphrase_type));
  migrated_nigori.set_keybag_is_frozen(true);

  if (!keystore_key_.empty()) {
    KeyParams key_params = {"localhost", "dummy", keystore_key_};
    if (!cryptographer->AddNonDefaultKey(key_params)) {
      LOG(ERROR) << "Failed to add keystore key as non-default key.";
      return false;
    }
  }
  if (new_passphrase_type == KEYSTORE_PASSPHRASE &&
      !GetKeystoreDecryptor(
          *cryptographer,
          keystore_key_,
          migrated_nigori.mutable_keystore_decryptor_token())) {
    LOG(ERROR) << "Failed to extract keystore decryptor token.";
    return false;
  }
  if (!cryptographer->GetKeys(migrated_nigori.mutable_encryption_keybag())) {
    LOG(ERROR) << "Failed to extract encryption keybag.";
    return false;
  }

  DVLOG(1) << "Completing nigori migration to keystore support.";
  nigori_node->SetNigoriSpecifics(migrated_nigori);
  if (passphrase_type_ != new_passphrase_type) {
    passphrase_type_ = new_passphrase_type;
    FOR_EACH_OBSERVER(SyncEncryptionHandler::Observer, observers_,
                      OnPassphraseTypeChanged(passphrase_type_));
  }
  if (new_encrypt_everything && !encrypt_everything_) {
    EnableEncryptEverythingImpl(trans->GetWrappedTrans());
    ReEncryptEverything(trans);
  }
  return true;
}

bool SyncEncryptionHandlerImpl::GetKeystoreDecryptor(
    const Cryptographer& cryptographer,
    const std::string& keystore_key,
    sync_pb::EncryptedData* encrypted_blob) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!keystore_key.empty());
  DCHECK(cryptographer.is_ready());
  std::string serialized_nigori;
  serialized_nigori = cryptographer.GetDefaultNigoriKey();
  if (serialized_nigori.empty()) {
    LOG(ERROR) << "Failed to get cryptographer bootstrap token.";
    return false;
  }
  Cryptographer temp_cryptographer(cryptographer.encryptor());
  KeyParams key_params = {"localhost", "dummy", keystore_key};
  if (!temp_cryptographer.AddKey(key_params))
    return false;
  if (!temp_cryptographer.EncryptString(serialized_nigori, encrypted_blob))
    return false;
  return true;
}

bool SyncEncryptionHandlerImpl::AttemptToInstallKeybag(
    const sync_pb::EncryptedData& keybag,
    bool update_default,
    Cryptographer* cryptographer) {
  if (!cryptographer->CanDecrypt(keybag))
    return false;
  cryptographer->InstallKeys(keybag);
  if (update_default)
    cryptographer->SetDefaultKey(keybag.key_name());
  return true;
}

void SyncEncryptionHandlerImpl::EnableEncryptEverythingImpl(
    syncable::BaseTransaction* const trans) {
  ModelTypeSet* encrypted_types = &UnlockVaultMutable(trans)->encrypted_types;
  if (encrypt_everything_) {
    DCHECK(encrypted_types->Equals(UserTypes()));
    return;
  }
  encrypt_everything_ = true;
  *encrypted_types = UserTypes();
  FOR_EACH_OBSERVER(
      Observer, observers_,
      OnEncryptedTypesChanged(*encrypted_types, encrypt_everything_));
}

bool SyncEncryptionHandlerImpl::DecryptPendingKeysWithKeystoreKey(
    const std::string& keystore_key,
    const sync_pb::EncryptedData& keystore_decryptor_token,
    Cryptographer* cryptographer) {
  DCHECK(cryptographer->has_pending_keys());
  if (keystore_decryptor_token.blob().empty())
    return false;
  Cryptographer temp_cryptographer(cryptographer->encryptor());
  KeyParams keystore_params = {"localhost", "dummy", keystore_key_};
  if (temp_cryptographer.AddKey(keystore_params) &&
      temp_cryptographer.CanDecrypt(keystore_decryptor_token)) {
    // Someone else migrated the nigori for us! How generous! Go ahead and
    // install both the keystore key and the new default encryption key
    // (i.e. the one provided by the keystore decryptor token) into the
    // cryptographer.
    // The keystore decryptor token is a keystore key encrypted blob containing
    // the current serialized default encryption key (and as such should be
    // able to decrypt the nigori node's encryption keybag).
    DVLOG(1) << "Attempting to decrypt pending keys using "
             << "keystore decryptor token.";
    std::string serialized_nigori =
        temp_cryptographer.DecryptToString(keystore_decryptor_token);
    // This will decrypt the pending keys and add them if possible. The key
    // within |serialized_nigori| will be the default after.
    cryptographer->ImportNigoriKey(serialized_nigori);
    // Theoretically the encryption keybag should already contain the keystore
    // key. We explicitly add it as a safety measure.
    cryptographer->AddNonDefaultKey(keystore_params);
    if (cryptographer->is_ready()) {
      std::string bootstrap_token;
      cryptographer->GetBootstrapToken(&bootstrap_token);
      DVLOG(1) << "Keystore decryptor token decrypted pending keys.";
      FOR_EACH_OBSERVER(
          SyncEncryptionHandler::Observer,
          observers_,
          OnBootstrapTokenUpdated(bootstrap_token,
                                  PASSPHRASE_BOOTSTRAP_TOKEN));
      FOR_EACH_OBSERVER(
          SyncEncryptionHandler::Observer,
          observers_,
          OnCryptographerStateChanged(cryptographer));
      return true;
    }
  }
  return false;
}

}  // namespace browser_sync
