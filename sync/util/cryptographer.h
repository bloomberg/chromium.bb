// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_UTIL_CRYPTOGRAPHER_H_
#define SYNC_UTIL_CRYPTOGRAPHER_H_

#include <map>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "sync/protocol/encryption.pb.h"
#include "sync/util/nigori.h"

namespace sync_pb {
class NigoriKeyBag;
class NigoriSpecifics;
}

namespace syncer {

class Encryptor;

extern const char kNigoriTag[];

// The parameters used to initialize a Nigori instance.
struct KeyParams {
  std::string hostname;
  std::string username;
  std::string password;
};

// This class manages the Nigori objects used to encrypt and decrypt sensitive
// sync data (eg. passwords). Each Nigori object knows how to handle data
// protected with a particular passphrase.
//
// Whenever an update to the Nigori sync node is received from the server,
// SetPendingKeys should be called with the encrypted contents of that node.
// Most likely, an updated Nigori node means that a new passphrase has been set
// and that future node updates won't be decryptable. To remedy this, the user
// should be prompted for the new passphrase and DecryptPendingKeys be called.
//
// Whenever a update to an encrypted node is received from the server,
// CanDecrypt should be used to verify whether the Cryptographer can decrypt
// that node. If it cannot, then the application of that update should be
// delayed until after it can be decrypted.
class Cryptographer {
 public:
  // Does not take ownership of |encryptor|.
  explicit Cryptographer(Encryptor* encryptor);
  ~Cryptographer();

  // |restored_bootstrap_token| can be provided via this method to bootstrap
  // Cryptographer instance into the ready state (is_ready will be true).
  // It must be a string that was previously built by the
  // GetSerializedBootstrapToken function.  It is possible that the token is no
  // longer valid (due to server key change), in which case the normal
  // decryption code paths will fail and the user will need to provide a new
  // passphrase.
  // It is an error to call this if is_ready() == true, though it is fair to
  // never call Bootstrap at all.
  void Bootstrap(const std::string& restored_bootstrap_token);

  // Bootstrap the keystore key.
  void BootstrapKeystoreKey(
      const std::string& restored_keystore_bootstrap_token);

  // Returns whether we can decrypt |encrypted| using the keys we currently know
  // about.
  bool CanDecrypt(const sync_pb::EncryptedData& encrypted) const;

  // Returns whether |encrypted| can be decrypted using the default encryption
  // key.
  bool CanDecryptUsingDefaultKey(const sync_pb::EncryptedData& encrypted) const;

  // Encrypts |message| into |encrypted|. Does not overwrite |encrypted| if
  // |message| already matches the decrypted data within |encrypted| and
  // |encrypted| was encrypted with the current default key. This avoids
  // unnecessarily modifying |encrypted| if the change had no practical effect.
  // Returns true unless encryption fails or |message| isn't valid (e.g. a
  // required field isn't set).
  bool Encrypt(const ::google::protobuf::MessageLite& message,
               sync_pb::EncryptedData* encrypted) const;

  // Decrypts |encrypted| into |message|. Returns true unless decryption fails,
  // or |message| fails to parse the decrypted data.
  bool Decrypt(const sync_pb::EncryptedData& encrypted,
               ::google::protobuf::MessageLite* message) const;

  // Decrypts |encrypted| and returns plaintext decrypted data. If decryption
  // fails, returns empty string.
  std::string DecryptToString(const sync_pb::EncryptedData& encrypted) const;

  // Encrypts the set of currently known keys into |encrypted|. Returns true if
  // successful.
  bool GetKeys(sync_pb::EncryptedData* encrypted) const;

  // Creates a new Nigori instance using |params|. If successful, |params| will
  // become the default encryption key and be used for all future calls to
  // Encrypt.
  bool AddKey(const KeyParams& params);

  // Same as AddKey(..), but builds the new Nigori from a previously persisted
  // bootstrap token. This can be useful when consuming a bootstrap token
  // with a cryptographer that has already been initialized.
  bool AddKeyFromBootstrapToken(const std::string restored_bootstrap_token);

  // Decrypts |encrypted| and uses its contents to initialize Nigori instances.
  // Returns true unless decryption of |encrypted| fails. The caller is
  // responsible for checking that CanDecrypt(encrypted) == true.
  // Does not update the default nigori.
  void InstallKeys(const sync_pb::EncryptedData& encrypted);


  // Makes a local copy of |encrypted| to later be decrypted by
  // DecryptPendingKeys. This should only be used if CanDecrypt(encrypted) ==
  // false.
  void SetPendingKeys(const sync_pb::EncryptedData& encrypted);

  // Makes |pending_keys_| available to callers that may want to cache its
  // value for later use on the UI thread. It is illegal to call this if the
  // cryptographer has no pending keys. Like other calls that access the
  // cryptographer, this method must be called from within a transaction.
  const sync_pb::EncryptedData& GetPendingKeys() const;

  // Attempts to decrypt the set of keys that was copied in the previous call to
  // SetPendingKeys using |params|. Returns true if the pending keys were
  // successfully decrypted and installed. If successful, the default key
  // is updated.
  bool DecryptPendingKeys(const KeyParams& params);

  // Sets the default key to the nigori with name |key_name|. |key_name| must
  // correspond to a nigori that has already been installed into the keybag.
  void SetDefaultKey(const std::string& key_name);

  bool is_initialized() const { return !nigoris_.empty() && default_nigori_; }

  // Returns whether this Cryptographer is ready to encrypt and decrypt data.
  bool is_ready() const { return is_initialized() &&
                          has_pending_keys() == false; }

  // Returns whether there is a pending set of keys that needs to be decrypted.
  bool has_pending_keys() const { return NULL != pending_keys_.get(); }

  // Obtain a token that can be provided on construction to a future
  // Cryptographer instance to bootstrap itself.  Returns false if such a token
  // can't be created (i.e. if this Cryptograhper doesn't have valid keys).
  bool GetBootstrapToken(std::string* token) const;

  // Obtain the bootstrap token based on the keystore encryption key.
  bool GetKeystoreKeyBootstrapToken(std::string* token) const;

  // Set the keystore-derived nigori from the provided key.
  // Returns true if we succesfully create the keystore derived nigori from the
  // provided key, false otherwise.
  bool SetKeystoreKey(const std::string& keystore_key);

  // Returns true if we currently have a keystore-derived nigori, false
  // otherwise.
  bool HasKeystoreKey() const;

  Encryptor* encryptor() const { return encryptor_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(SyncCryptographerTest, PackUnpack);

  typedef std::map<std::string, linked_ptr<const Nigori> > NigoriMap;

  // Helper method to instantiate Nigori instances for each set of key
  // parameters in |bag|.
  // Does not update the default nigori.
  void InstallKeyBag(const sync_pb::NigoriKeyBag& bag);

  // Helper method to add a nigori as either the new default nigori or the new
  // keystore nigori.
  bool AddKeyImpl(Nigori* nigori, bool is_keystore_key);

  // Functions to serialize + encrypt a Nigori object in an opaque format for
  // persistence by sync infrastructure.
  bool PackBootstrapToken(const Nigori* nigori, std::string* pack_into) const;
  Nigori* UnpackBootstrapToken(const std::string& token) const;

  Encryptor* const encryptor_;

  NigoriMap nigoris_;  // The Nigoris we know about, mapped by key name.
  NigoriMap::value_type* default_nigori_;  // The Nigori used for encryption.
  NigoriMap::value_type* keystore_nigori_; // Nigori generated from keystore.

  scoped_ptr<sync_pb::EncryptedData> pending_keys_;

  DISALLOW_COPY_AND_ASSIGN(Cryptographer);
};

}  // namespace syncer

#endif  // SYNC_UTIL_CRYPTOGRAPHER_H_
