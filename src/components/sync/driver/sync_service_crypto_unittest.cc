// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_service_crypto.h"

#include <list>
#include <map>
#include <utility>

#include "base/bind_helpers.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/driver/trusted_vault_client.h"
#include "components/sync/engine/mock_sync_engine.h"
#include "components/sync/nigori/nigori.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::_;
using testing::Eq;

sync_pb::EncryptedData MakeEncryptedData(
    const std::string& passphrase,
    const KeyDerivationParams& derivation_params) {
  std::unique_ptr<Nigori> nigori =
      Nigori::CreateByDerivation(derivation_params, passphrase);

  std::string nigori_name;
  EXPECT_TRUE(
      nigori->Permute(Nigori::Type::Password, kNigoriKeyName, &nigori_name));

  const std::string unencrypted = "test";
  sync_pb::EncryptedData encrypted;
  encrypted.set_key_name(nigori_name);
  EXPECT_TRUE(nigori->Encrypt(unencrypted, encrypted.mutable_blob()));
  return encrypted;
}

CoreAccountInfo MakeAccountInfoWithGaia(const std::string& gaia) {
  CoreAccountInfo result;
  result.gaia = gaia;
  return result;
}

class MockCryptoSyncPrefs : public CryptoSyncPrefs {
 public:
  MockCryptoSyncPrefs() = default;
  ~MockCryptoSyncPrefs() override = default;

  MOCK_CONST_METHOD0(GetEncryptionBootstrapToken, std::string());
  MOCK_METHOD1(SetEncryptionBootstrapToken, void(const std::string&));
  MOCK_CONST_METHOD0(GetKeystoreEncryptionBootstrapToken, std::string());
  MOCK_METHOD1(SetKeystoreEncryptionBootstrapToken, void(const std::string&));
};

// Object representing a server that contains the authoritative trusted vault
// keys, and TestTrustedVaultClient reads from.
class TestTrustedVaultServer {
 public:
  TestTrustedVaultServer() = default;
  ~TestTrustedVaultServer() = default;

  void StoreKeysOnServer(const std::string& gaia_id,
                         const std::vector<std::vector<uint8_t>>& keys) {
    gaia_id_to_keys_[gaia_id] = keys;
  }

  // Mimics a user going through a key-retrieval flow (e.g. reauth) such that
  // keys are fetched from the server and cached in |client|.
  void MimicKeyRetrievalByUser(const std::string& gaia_id,
                               TrustedVaultClient* client) {
    DCHECK(client);
    DCHECK_NE(0U, gaia_id_to_keys_.count(gaia_id))
        << "StoreKeysOnServer() should have been called for " << gaia_id;

    client->StoreKeys(gaia_id, gaia_id_to_keys_[gaia_id],
                      /*last_key_version=*/
                      static_cast<int>(gaia_id_to_keys_[gaia_id].size()) - 1);
  }

  // Mimics the server RPC endpoint that allows key rotation.
  std::vector<std::vector<uint8_t>> RequestRotatedKeysFromServer(
      const std::string& gaia_id,
      const std::vector<uint8_t>& key_known_by_client) const {
    auto it = gaia_id_to_keys_.find(gaia_id);
    if (it == gaia_id_to_keys_.end()) {
      return {};
    }

    const std::vector<std::vector<uint8_t>>& latest_keys = it->second;
    if (std::find(latest_keys.begin(), latest_keys.end(),
                  key_known_by_client) == latest_keys.end()) {
      // |key_known_by_client| is invalid or too old: cannot be used to follow
      // key rotation.
      return {};
    }

    return latest_keys;
  }

 private:
  std::map<std::string, std::vector<std::vector<uint8_t>>> gaia_id_to_keys_;
};

// Simple in-memory implementation of TrustedVaultClient.
class TestTrustedVaultClient : public TrustedVaultClient {
 public:
  explicit TestTrustedVaultClient(const TestTrustedVaultServer* server)
      : server_(server) {}

  ~TestTrustedVaultClient() override = default;

  // Exposes the total number of calls to FetchKeys().
  int fetch_count() const { return fetch_count_; }

  // Exposes the total number of calls to MarkKeysAsStale().
  bool keys_marked_as_stale_count() const {
    return keys_marked_as_stale_count_;
  }

  // Exposes the total number of calls to the server's RequestKeysFromServer().
  int server_request_count() const { return server_request_count_; }

  // Mimics the completion of the next (FIFO) FetchKeys() request.
  bool CompleteFetchKeysRequest() {
    if (pending_responses_.empty()) {
      return false;
    }

    base::OnceClosure cb = std::move(pending_responses_.front());
    pending_responses_.pop_front();
    std::move(cb).Run();
    return true;
  }

  // TrustedVaultClient implementation.
  std::unique_ptr<Subscription> AddKeysChangedObserver(
      const base::RepeatingClosure& cb) override {
    return observer_list_.Add(cb);
  }

  void FetchKeys(
      const CoreAccountInfo& account_info,
      base::OnceCallback<void(const std::vector<std::vector<uint8_t>>&)> cb)
      override {
    const std::string& gaia_id = account_info.gaia;

    ++fetch_count_;

    CachedKeysPerUser& cached_keys = gaia_id_to_cached_keys_[gaia_id];

    // If there are no keys cached, the only way to bootstrap the client is by
    // going through a retrieval flow, see MimicKeyRetrievalByUser().
    if (cached_keys.keys.empty()) {
      pending_responses_.push_back(
          base::BindOnce(std::move(cb), std::vector<std::vector<uint8_t>>()));
      return;
    }

    // If the locally cached keys are not marked as stale, return them directly.
    if (!cached_keys.marked_as_stale) {
      pending_responses_.push_back(
          base::BindOnce(std::move(cb), cached_keys.keys));
      return;
    }

    // Fetch keys from the server and cache them.
    cached_keys.keys =
        server_->RequestRotatedKeysFromServer(gaia_id, cached_keys.keys.back());
    cached_keys.marked_as_stale = false;

    // Return the newly-cached keys.
    pending_responses_.push_back(
        base::BindOnce(std::move(cb), cached_keys.keys));
  }

  // Store keys in the client-side cache, usually retrieved from the server as
  // part of the key retrieval process, see MimicKeyRetrievalByUser().
  void StoreKeys(const std::string& gaia_id,
                 const std::vector<std::vector<uint8_t>>& keys,
                 int last_key_version) override {
    CachedKeysPerUser& cached_keys = gaia_id_to_cached_keys_[gaia_id];
    cached_keys.keys = keys;
    cached_keys.marked_as_stale = false;
    observer_list_.Notify();
  }

  void RemoveAllStoredKeys() override {
    gaia_id_to_cached_keys_.clear();
    observer_list_.Notify();
  }

  void MarkKeysAsStale(const CoreAccountInfo& account_info,
                       base::OnceCallback<void(bool)> cb) override {
    const std::string& gaia_id = account_info.gaia;

    ++keys_marked_as_stale_count_;

    CachedKeysPerUser& cached_keys = gaia_id_to_cached_keys_[gaia_id];

    if (cached_keys.keys.empty() || cached_keys.marked_as_stale) {
      // Nothing changed so report |false|.
      std::move(cb).Run(false);
      return;
    }

    // The cache is stale and should be invalidated. Following calls to
    // FetchKeys() will read from the server.
    cached_keys.marked_as_stale = true;
    std::move(cb).Run(true);
  }

 private:
  struct CachedKeysPerUser {
    bool marked_as_stale = false;
    std::vector<std::vector<uint8_t>> keys;
  };

  const TestTrustedVaultServer* const server_;

  std::map<std::string, CachedKeysPerUser> gaia_id_to_cached_keys_;
  CallbackList observer_list_;
  int fetch_count_ = 0;
  int keys_marked_as_stale_count_ = 0;
  int server_request_count_ = 0;
  std::list<base::OnceClosure> pending_responses_;
};

class SyncServiceCryptoTest : public testing::Test {
 protected:
  // Account used in most tests.
  const CoreAccountInfo kSyncingAccount =
      MakeAccountInfoWithGaia("syncingaccount");

  // Initial trusted vault keys stored on the server |TestTrustedVaultServer|
  // for |kSyncingAccount|.
  const std::vector<std::vector<uint8_t>> kInitialTrustedVaultKeys = {
      {0, 1, 2, 3, 4}};

  SyncServiceCryptoTest()
      : trusted_vault_client_(&trusted_vault_server_),
        crypto_(notify_observers_cb_.Get(),
                /*notify_required_user_action_changed=*/base::DoNothing(),
                reconfigure_cb_.Get(),
                &prefs_,
                &trusted_vault_client_) {
    trusted_vault_server_.StoreKeysOnServer(kSyncingAccount.gaia,
                                            kInitialTrustedVaultKeys);
  }

  ~SyncServiceCryptoTest() override = default;

  bool VerifyAndClearExpectations() {
    return testing::Mock::VerifyAndClearExpectations(&notify_observers_cb_) &&
           testing::Mock::VerifyAndClearExpectations(&notify_observers_cb_) &&
           testing::Mock::VerifyAndClearExpectations(&trusted_vault_client_) &&
           testing::Mock::VerifyAndClearExpectations(&engine_);
  }

  void MimicKeyRetrievalByUser() {
    trusted_vault_server_.MimicKeyRetrievalByUser(kSyncingAccount.gaia,
                                                  &trusted_vault_client_);
  }

  testing::NiceMock<base::MockCallback<base::RepeatingClosure>>
      notify_observers_cb_;
  testing::NiceMock<
      base::MockCallback<base::RepeatingCallback<void(ConfigureReason)>>>
      reconfigure_cb_;
  testing::NiceMock<MockCryptoSyncPrefs> prefs_;
  TestTrustedVaultServer trusted_vault_server_;
  TestTrustedVaultClient trusted_vault_client_;
  testing::NiceMock<MockSyncEngine> engine_;
  SyncServiceCrypto crypto_;
};

TEST_F(SyncServiceCryptoTest, ShouldExposePassphraseRequired) {
  const std::string kTestPassphrase = "somepassphrase";

  crypto_.SetSyncEngine(CoreAccountInfo(), &engine_);
  ASSERT_FALSE(crypto_.IsPassphraseRequired());
  ASSERT_THAT(trusted_vault_client_.fetch_count(), Eq(0));

  // Mimic the engine determining that a passphrase is required.
  EXPECT_CALL(reconfigure_cb_, Run(CONFIGURE_REASON_CRYPTO));
  crypto_.OnPassphraseRequired(
      REASON_DECRYPTION, KeyDerivationParams::CreateForPbkdf2(),
      MakeEncryptedData(kTestPassphrase,
                        KeyDerivationParams::CreateForPbkdf2()));
  EXPECT_TRUE(crypto_.IsPassphraseRequired());
  VerifyAndClearExpectations();

  // Entering the wrong passphrase should be rejected.
  EXPECT_CALL(reconfigure_cb_, Run(_)).Times(0);
  EXPECT_CALL(engine_, SetDecryptionPassphrase(_)).Times(0);
  EXPECT_FALSE(crypto_.SetDecryptionPassphrase("wrongpassphrase"));
  EXPECT_TRUE(crypto_.IsPassphraseRequired());

  // Entering the correct passphrase should be accepted.
  EXPECT_CALL(engine_, SetDecryptionPassphrase(kTestPassphrase))
      .WillOnce([&](const std::string&) { crypto_.OnPassphraseAccepted(); });
  // The current implementation issues two reconfigurations: one immediately
  // after checking the passphase in the UI thread and a second time later when
  // the engine confirms with OnPassphraseAccepted().
  EXPECT_CALL(reconfigure_cb_, Run(CONFIGURE_REASON_CRYPTO)).Times(2);
  EXPECT_TRUE(crypto_.SetDecryptionPassphrase(kTestPassphrase));
  EXPECT_FALSE(crypto_.IsPassphraseRequired());
}

TEST_F(SyncServiceCryptoTest,
       ShouldReadValidTrustedVaultKeysFromClientBeforeInitialization) {
  // Cache |kInitialTrustedVaultKeys| into |trusted_vault_client_| prior to
  // engine initialization.
  MimicKeyRetrievalByUser();

  EXPECT_CALL(reconfigure_cb_, Run(_)).Times(0);
  ASSERT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  // OnTrustedVaultKeyRequired() called during initialization of the sync
  // engine (i.e. before SetSyncEngine()).
  crypto_.OnTrustedVaultKeyRequired();

  // Trusted vault keys should be fetched only after the engine initialization
  // is completed.
  ASSERT_THAT(trusted_vault_client_.fetch_count(), Eq(0));
  crypto_.SetSyncEngine(kSyncingAccount, &engine_);

  // While there is an ongoing fetch, there should be no user action required.
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(1));
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  base::OnceClosure add_keys_cb;
  EXPECT_CALL(engine_,
              AddTrustedVaultDecryptionKeys(kInitialTrustedVaultKeys, _))
      .WillOnce(
          [&](const std::vector<std::vector<uint8_t>>& keys,
              base::OnceClosure done_cb) { add_keys_cb = std::move(done_cb); });

  // Mimic completion of the fetch.
  ASSERT_TRUE(trusted_vault_client_.CompleteFetchKeysRequest());
  ASSERT_TRUE(add_keys_cb);
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  // Mimic completion of the engine.
  EXPECT_CALL(reconfigure_cb_, Run(CONFIGURE_REASON_CRYPTO));
  crypto_.OnTrustedVaultKeyAccepted();
  std::move(add_keys_cb).Run();
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(1));
  EXPECT_THAT(trusted_vault_client_.keys_marked_as_stale_count(), Eq(0));
  EXPECT_THAT(trusted_vault_client_.server_request_count(), Eq(0));
}

TEST_F(SyncServiceCryptoTest,
       ShouldReadValidTrustedVaultKeysFromClientAfterInitialization) {
  // Cache |kInitialTrustedVaultKeys| into |trusted_vault_client_| prior to
  // engine initialization.
  MimicKeyRetrievalByUser();

  EXPECT_CALL(reconfigure_cb_, Run(_)).Times(0);
  ASSERT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  // Mimic the initialization of the sync engine, without trusted vault keys
  // being required.
  crypto_.SetSyncEngine(kSyncingAccount, &engine_);
  ASSERT_THAT(trusted_vault_client_.fetch_count(), Eq(0));

  // Later on, mimic trusted vault keys being required (e.g. remote Nigori
  // update), which should trigger a fetch.
  crypto_.OnTrustedVaultKeyRequired();
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(1));

  // While there is an ongoing fetch, there should be no user action required.
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  base::OnceClosure add_keys_cb;
  EXPECT_CALL(engine_,
              AddTrustedVaultDecryptionKeys(kInitialTrustedVaultKeys, _))
      .WillOnce(
          [&](const std::vector<std::vector<uint8_t>>& keys,
              base::OnceClosure done_cb) { add_keys_cb = std::move(done_cb); });

  // Mimic completion of the fetch.
  ASSERT_TRUE(trusted_vault_client_.CompleteFetchKeysRequest());
  ASSERT_TRUE(add_keys_cb);
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  // Mimic completion of the engine.
  EXPECT_CALL(reconfigure_cb_, Run(CONFIGURE_REASON_CRYPTO));
  crypto_.OnTrustedVaultKeyAccepted();
  std::move(add_keys_cb).Run();
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(1));
  EXPECT_THAT(trusted_vault_client_.keys_marked_as_stale_count(), Eq(0));
  EXPECT_THAT(trusted_vault_client_.server_request_count(), Eq(0));
}

TEST_F(SyncServiceCryptoTest,
       ShouldReadNoTrustedVaultKeysFromClientAfterInitialization) {
  EXPECT_CALL(reconfigure_cb_, Run(_)).Times(0);
  EXPECT_CALL(engine_, AddTrustedVaultDecryptionKeys(_, _)).Times(0);

  ASSERT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  // Mimic the initialization of the sync engine, without trusted vault keys
  // being required.
  crypto_.SetSyncEngine(kSyncingAccount, &engine_);
  ASSERT_THAT(trusted_vault_client_.fetch_count(), Eq(0));
  ASSERT_THAT(trusted_vault_client_.server_request_count(), Eq(0));

  // Later on, mimic trusted vault keys being required (e.g. remote Nigori
  // update), which should trigger a fetch.
  crypto_.OnTrustedVaultKeyRequired();
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(1));

  // While there is an ongoing fetch, there should be no user action required.
  ASSERT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  // Mimic completion of the fetch, which should lead to a reconfiguration.
  EXPECT_CALL(reconfigure_cb_, Run(CONFIGURE_REASON_CRYPTO));
  ASSERT_TRUE(trusted_vault_client_.CompleteFetchKeysRequest());
  EXPECT_TRUE(crypto_.IsTrustedVaultKeyRequired());
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(1));

  EXPECT_THAT(trusted_vault_client_.server_request_count(), Eq(0));
  EXPECT_THAT(trusted_vault_client_.keys_marked_as_stale_count(), Eq(0));
}

TEST_F(SyncServiceCryptoTest, ShouldReadInvalidTrustedVaultKeysFromClient) {
  // Cache |kInitialTrustedVaultKeys| into |trusted_vault_client_| prior to
  // engine initialization. In this test, |kInitialTrustedVaultKeys| does not
  // match the Nigori keys (i.e. the engine continues to think trusted vault
  // keys are required).
  MimicKeyRetrievalByUser();

  base::OnceClosure add_keys_cb;
  ON_CALL(engine_, AddTrustedVaultDecryptionKeys(_, _))
      .WillByDefault(
          [&](const std::vector<std::vector<uint8_t>>& keys,
              base::OnceClosure done_cb) { add_keys_cb = std::move(done_cb); });

  ASSERT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  // Mimic the initialization of the sync engine, without trusted vault keys
  // being required.
  crypto_.SetSyncEngine(kSyncingAccount, &engine_);
  ASSERT_THAT(trusted_vault_client_.fetch_count(), Eq(0));
  ASSERT_THAT(trusted_vault_client_.server_request_count(), Eq(0));

  // Later on, mimic trusted vault keys being required (e.g. remote Nigori
  // update), which should trigger a fetch.
  crypto_.OnTrustedVaultKeyRequired();
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(1));

  // While there is an ongoing fetch, there should be no user action required.
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  // Mimic completion of the client.
  EXPECT_CALL(engine_,
              AddTrustedVaultDecryptionKeys(kInitialTrustedVaultKeys, _));
  ASSERT_TRUE(trusted_vault_client_.CompleteFetchKeysRequest());
  ASSERT_TRUE(add_keys_cb);
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  // Mimic completion of the engine, without OnTrustedVaultKeyAccepted().
  std::move(add_keys_cb).Run();

  // The keys should be marked as stale, and a second fetch attempt started.
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());
  EXPECT_THAT(trusted_vault_client_.keys_marked_as_stale_count(), Eq(1));
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(2));

  // Mimic completion of the client for the second pass.
  EXPECT_CALL(engine_,
              AddTrustedVaultDecryptionKeys(kInitialTrustedVaultKeys, _));
  ASSERT_TRUE(trusted_vault_client_.CompleteFetchKeysRequest());
  ASSERT_TRUE(add_keys_cb);

  // Mimic completion of the engine, without OnTrustedVaultKeyAccepted(), for
  // the second pass.
  EXPECT_CALL(reconfigure_cb_, Run(CONFIGURE_REASON_CRYPTO));
  std::move(add_keys_cb).Run();

  EXPECT_TRUE(crypto_.IsTrustedVaultKeyRequired());
  EXPECT_THAT(trusted_vault_client_.keys_marked_as_stale_count(), Eq(1));
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(2));
}

// Similar to ShouldReadInvalidTrustedVaultKeysFromClient but in this case the
// client is able to follow a key rotation as part of the second fetch attempt.
TEST_F(SyncServiceCryptoTest, ShouldFollowKeyRotationDueToSecondFetch) {
  const std::vector<std::vector<uint8_t>> kRotatedKeys = {
      kInitialTrustedVaultKeys[0], {2, 3, 4, 5}};

  // Cache |kInitialTrustedVaultKeys| into |trusted_vault_client_| prior to
  // engine initialization. In this test, |kInitialTrustedVaultKeys| does not
  // match the Nigori keys (i.e. the engine continues to think trusted vault
  // keys are required until |kRotatedKeys| are provided).
  MimicKeyRetrievalByUser();

  // Mimic server-side key rotation which the keys, in a way that the rotated
  // keys are a continuation of kInitialTrustedVaultKeys, such that
  // TestTrustedVaultServer will allow the client to silently follow key
  // rotation.
  trusted_vault_server_.StoreKeysOnServer(kSyncingAccount.gaia, kRotatedKeys);

  // The engine replies with OnTrustedVaultKeyAccepted() only if |kRotatedKeys|
  // are provided.
  ON_CALL(engine_, AddTrustedVaultDecryptionKeys(_, _))
      .WillByDefault([&](const std::vector<std::vector<uint8_t>>& keys,
                         base::OnceClosure done_cb) {
        if (keys == kRotatedKeys) {
          crypto_.OnTrustedVaultKeyAccepted();
        }
        std::move(done_cb).Run();
      });

  // Mimic initialization of the engine where trusted vault keys are needed and
  // |kInitialTrustedVaultKeys| are fetched as part of the first fetch.
  crypto_.SetSyncEngine(kSyncingAccount, &engine_);
  crypto_.OnTrustedVaultKeyRequired();
  ASSERT_THAT(trusted_vault_client_.fetch_count(), Eq(1));

  // While there is an ongoing fetch (first attempt), there should be no user
  // action required.
  ASSERT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  // The keys fetched in the first attempt (|kInitialTrustedVaultKeys|) are
  // insufficient and should be marked as stale. In addition, a second fetch
  // should be triggered.
  ASSERT_TRUE(trusted_vault_client_.CompleteFetchKeysRequest());
  ASSERT_THAT(trusted_vault_client_.keys_marked_as_stale_count(), Eq(1));
  ASSERT_THAT(trusted_vault_client_.fetch_count(), Eq(2));

  // While there is an ongoing fetch (second attempt), there should be no user
  // action required.
  ASSERT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  // Because of |kRotatedKeys| is a continuation of |kInitialTrustedVaultKeys|,
  // TrustedVaultServer should successfully deliver the new keys |kRotatedKeys|
  // to the client.
  EXPECT_CALL(reconfigure_cb_, Run(CONFIGURE_REASON_CRYPTO));
  ASSERT_TRUE(trusted_vault_client_.CompleteFetchKeysRequest());
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());
  ASSERT_THAT(trusted_vault_client_.keys_marked_as_stale_count(), Eq(1));
}

// Similar to ShouldReadInvalidTrustedVaultKeysFromClient: the vault
// initially has no valid keys, leading to IsTrustedVaultKeyRequired().
// Later, the vault gets populated with the keys, which should trigger
// a fetch and eventually resolve the encryption issue.
TEST_F(SyncServiceCryptoTest, ShouldRefetchTrustedVaultKeysWhenChangeObserved) {
  const std::vector<std::vector<uint8_t>> kNewKeys = {{2, 3, 4, 5}};

  // Cache |kInitialTrustedVaultKeys| into |trusted_vault_client_| prior to
  // engine initialization. In this test, |kInitialTrustedVaultKeys| does not
  // match the Nigori keys (i.e. the engine continues to think trusted vault
  // keys are required until |kNewKeys| are provided).
  MimicKeyRetrievalByUser();

  // The engine replies with OnTrustedVaultKeyAccepted() only if |kNewKeys| are
  // provided.
  ON_CALL(engine_, AddTrustedVaultDecryptionKeys(_, _))
      .WillByDefault([&](const std::vector<std::vector<uint8_t>>& keys,
                         base::OnceClosure done_cb) {
        if (keys == kNewKeys) {
          crypto_.OnTrustedVaultKeyAccepted();
        }
        std::move(done_cb).Run();
      });

  // Mimic initialization of the engine where trusted vault keys are needed and
  // |kInitialTrustedVaultKeys| are fetched, which are insufficient, and hence
  // IsTrustedVaultKeyRequired() is exposed.
  crypto_.SetSyncEngine(kSyncingAccount, &engine_);
  crypto_.OnTrustedVaultKeyRequired();
  ASSERT_THAT(trusted_vault_client_.fetch_count(), Eq(1));
  ASSERT_TRUE(trusted_vault_client_.CompleteFetchKeysRequest());
  // Note that this initial attempt involves two fetches, where both return
  // |kInitialTrustedVaultKeys|.
  ASSERT_THAT(trusted_vault_client_.keys_marked_as_stale_count(), Eq(1));
  ASSERT_THAT(trusted_vault_client_.fetch_count(), Eq(2));
  ASSERT_TRUE(trusted_vault_client_.CompleteFetchKeysRequest());
  ASSERT_THAT(trusted_vault_client_.keys_marked_as_stale_count(), Eq(1));
  ASSERT_TRUE(crypto_.IsTrustedVaultKeyRequired());

  // Mimic server-side key reset and a new retrieval.
  trusted_vault_server_.StoreKeysOnServer(kSyncingAccount.gaia, kNewKeys);
  MimicKeyRetrievalByUser();

  // Key retrieval should have initiated a third fetch.
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(3));
  EXPECT_CALL(reconfigure_cb_, Run(CONFIGURE_REASON_CRYPTO));
  EXPECT_TRUE(trusted_vault_client_.CompleteFetchKeysRequest());
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());
  EXPECT_THAT(trusted_vault_client_.keys_marked_as_stale_count(), Eq(1));
}

// Same as above but the new keys become available during an ongoing FetchKeys()
// request.
TEST_F(SyncServiceCryptoTest,
       ShouldDeferTrustedVaultKeyFetchingWhenChangeObservedWhileOngoingFetch) {
  const std::vector<std::vector<uint8_t>> kNewKeys = {{2, 3, 4, 5}};

  // Cache |kInitialTrustedVaultKeys| into |trusted_vault_client_| prior to
  // engine initialization. In this test, |kInitialTrustedVaultKeys| does not
  // match the Nigori keys (i.e. the engine continues to think trusted vault
  // keys are required until |kNewKeys| are provided).
  MimicKeyRetrievalByUser();

  // The engine replies with OnTrustedVaultKeyAccepted() only if |kNewKeys| are
  // provided.
  ON_CALL(engine_, AddTrustedVaultDecryptionKeys(_, _))
      .WillByDefault([&](const std::vector<std::vector<uint8_t>>& keys,
                         base::OnceClosure done_cb) {
        if (keys == kNewKeys) {
          crypto_.OnTrustedVaultKeyAccepted();
        }
        std::move(done_cb).Run();
      });

  // Mimic initialization of the engine where trusted vault keys are needed and
  // |kInitialTrustedVaultKeys| are in the process of being fetched.
  crypto_.SetSyncEngine(kSyncingAccount, &engine_);
  crypto_.OnTrustedVaultKeyRequired();
  ASSERT_THAT(trusted_vault_client_.fetch_count(), Eq(1));
  ASSERT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  // While there is an ongoing fetch, mimic server-side key reset and a new
  // retrieval.
  trusted_vault_server_.StoreKeysOnServer(kSyncingAccount.gaia, kNewKeys);
  MimicKeyRetrievalByUser();

  // Because there's already an ongoing fetch, a second one should not have been
  // triggered yet and should be deferred instead.
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(1));

  // As soon as the first fetch completes, the second one (deferred) should be
  // started.
  EXPECT_TRUE(trusted_vault_client_.CompleteFetchKeysRequest());
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(2));
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());

  // The completion of the second fetch should resolve the encryption issue.
  EXPECT_CALL(reconfigure_cb_, Run(CONFIGURE_REASON_CRYPTO));
  EXPECT_TRUE(trusted_vault_client_.CompleteFetchKeysRequest());
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(2));
  EXPECT_FALSE(crypto_.IsTrustedVaultKeyRequired());
}

// The engine gets initialized and the vault initially has insufficient keys,
// leading to IsTrustedVaultKeyRequired(). Later, keys are added to the vault
// *twice*, where the later event should be handled as a deferred fetch.
TEST_F(
    SyncServiceCryptoTest,
    ShouldDeferTrustedVaultKeyFetchingWhenChangeObservedWhileOngoingRefetch) {
  const std::vector<std::vector<uint8_t>> kLatestKeys = {{2, 2, 2, 2, 2}};

  // The engine replies with OnTrustedVaultKeyAccepted() only if |kLatestKeys|
  // are provided.
  ON_CALL(engine_, AddTrustedVaultDecryptionKeys(_, _))
      .WillByDefault([&](const std::vector<std::vector<uint8_t>>& keys,
                         base::OnceClosure done_cb) {
        if (keys == kLatestKeys) {
          crypto_.OnTrustedVaultKeyAccepted();
        }
        std::move(done_cb).Run();
      });

  // Mimic initialization of the engine where trusted vault keys are needed and
  // no keys are fetched from the client, hence IsTrustedVaultKeyRequired() is
  // exposed.
  crypto_.SetSyncEngine(kSyncingAccount, &engine_);
  crypto_.OnTrustedVaultKeyRequired();
  ASSERT_THAT(trusted_vault_client_.fetch_count(), Eq(1));
  ASSERT_TRUE(trusted_vault_client_.CompleteFetchKeysRequest());
  ASSERT_THAT(trusted_vault_client_.fetch_count(), Eq(1));
  ASSERT_THAT(trusted_vault_client_.keys_marked_as_stale_count(), Eq(0));
  ASSERT_TRUE(crypto_.IsTrustedVaultKeyRequired());

  // Mimic retrieval of keys, leading to a second fetch that returns
  // |kInitialTrustedVaultKeys|, which are insufficient and should be marked as
  // stale as soon as the fetch completes (later below).
  MimicKeyRetrievalByUser();
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(2));

  // While the second fetch is ongoing, mimic additional keys being retrieved.
  // Because there's already an ongoing fetch, a third one should not have been
  // triggered yet and should be deferred instead.
  trusted_vault_server_.StoreKeysOnServer(kSyncingAccount.gaia, kLatestKeys);
  MimicKeyRetrievalByUser();
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(2));

  // As soon as the second fetch completes, the keys should be marked as stale
  // and a third fetch attempt triggered.
  EXPECT_TRUE(trusted_vault_client_.CompleteFetchKeysRequest());
  EXPECT_THAT(trusted_vault_client_.keys_marked_as_stale_count(), Eq(1));
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(3));

  // As soon as the third fetch completes, the fourth one (deferred) should be
  // started.
  EXPECT_TRUE(trusted_vault_client_.CompleteFetchKeysRequest());
  EXPECT_THAT(trusted_vault_client_.fetch_count(), Eq(3));
}

}  // namespace

}  // namespace syncer
