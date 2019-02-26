// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_CRYPTO_QUIC_CRYPTO_CLIENT_CONFIG_H_
#define NET_THIRD_PARTY_QUIC_CORE_CRYPTO_QUIC_CRYPTO_CLIENT_CONFIG_H_

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "net/third_party/quic/core/crypto/crypto_handshake.h"
#include "net/third_party/quic/core/quic_packets.h"
#include "net/third_party/quic/core/quic_server_id.h"
#include "net/third_party/quic/platform/api/quic_export.h"
#include "net/third_party/quic/platform/api/quic_reference_counted.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"
#include "third_party/boringssl/src/include/openssl/base.h"

namespace quic {

class ChannelIDKey;
class ChannelIDSource;
class CryptoHandshakeMessage;
class ProofVerifier;
class ProofVerifyDetails;
class QuicRandom;

// QuicCryptoClientConfig contains crypto-related configuration settings for a
// client. Note that this object isn't thread-safe. It's designed to be used on
// a single thread at a time.
class QUIC_EXPORT_PRIVATE QuicCryptoClientConfig : public QuicCryptoConfig {
 public:
  // A CachedState contains the information that the client needs in order to
  // perform a 0-RTT handshake with a server. This information can be reused
  // over several connections to the same server.
  class QUIC_EXPORT_PRIVATE CachedState {
   public:
    // Enum to track if the server config is valid or not. If it is not valid,
    // it specifies why it is invalid.
    enum ServerConfigState {
      // WARNING: Do not change the numerical values of any of server config
      // state. Do not remove deprecated server config states - just comment
      // them as deprecated.
      SERVER_CONFIG_EMPTY = 0,
      SERVER_CONFIG_INVALID = 1,
      SERVER_CONFIG_CORRUPTED = 2,
      SERVER_CONFIG_EXPIRED = 3,
      SERVER_CONFIG_INVALID_EXPIRY = 4,
      SERVER_CONFIG_VALID = 5,
      // NOTE: Add new server config states only immediately above this line.
      // Make sure to update the QuicServerConfigState enum in
      // tools/metrics/histograms/histograms.xml accordingly.
      SERVER_CONFIG_COUNT
    };

    CachedState();
    CachedState(const CachedState&) = delete;
    CachedState& operator=(const CachedState&) = delete;
    ~CachedState();

    // IsComplete returns true if this object contains enough information to
    // perform a handshake with the server. |now| is used to judge whether any
    // cached server config has expired.
    bool IsComplete(QuicWallTime now) const;

    // IsEmpty returns true if |server_config_| is empty.
    bool IsEmpty() const;

    // GetServerConfig returns the parsed contents of |server_config|, or
    // nullptr if |server_config| is empty. The return value is owned by this
    // object and is destroyed when this object is.
    const CryptoHandshakeMessage* GetServerConfig() const;

    // SetServerConfig checks that |server_config| parses correctly and stores
    // it in |server_config_|. |now| is used to judge whether |server_config|
    // has expired.
    ServerConfigState SetServerConfig(QuicStringPiece server_config,
                                      QuicWallTime now,
                                      QuicWallTime expiry_time,
                                      QuicString* error_details);

    // InvalidateServerConfig clears the cached server config (if any).
    void InvalidateServerConfig();

    // SetProof stores a cert chain, cert signed timestamp and signature.
    void SetProof(const std::vector<QuicString>& certs,
                  QuicStringPiece cert_sct,
                  QuicStringPiece chlo_hash,
                  QuicStringPiece signature);

    // Clears all the data.
    void Clear();

    // Clears the certificate chain and signature and invalidates the proof.
    void ClearProof();

    // SetProofValid records that the certificate chain and signature have been
    // validated and that it's safe to assume that the server is legitimate.
    // (Note: this does not check the chain or signature.)
    void SetProofValid();

    // If the server config or the proof has changed then it needs to be
    // revalidated. Helper function to keep server_config_valid_ and
    // generation_counter_ in sync.
    void SetProofInvalid();

    const QuicString& server_config() const;
    const QuicString& source_address_token() const;
    const std::vector<QuicString>& certs() const;
    const QuicString& cert_sct() const;
    const QuicString& chlo_hash() const;
    const QuicString& signature() const;
    bool proof_valid() const;
    uint64_t generation_counter() const;
    const ProofVerifyDetails* proof_verify_details() const;

    void set_source_address_token(QuicStringPiece token);

    void set_cert_sct(QuicStringPiece cert_sct);

    // Adds the connection ID to the queue of server-designated connection-ids.
    void add_server_designated_connection_id(QuicConnectionId connection_id);

    // If true, the crypto config contains at least one connection ID specified
    // by the server, and the client should use one of these IDs when initiating
    // the next connection.
    bool has_server_designated_connection_id() const;

    // This function should only be called when
    // has_server_designated_connection_id is true.  Returns the next
    // connection_id specified by the server and removes it from the
    // queue of ids.
    QuicConnectionId GetNextServerDesignatedConnectionId();

    // Adds the servernonce to the queue of server nonces.
    void add_server_nonce(const QuicString& server_nonce);

    // If true, the crypto config contains at least one server nonce, and the
    // client should use one of these nonces.
    bool has_server_nonce() const;

    // This function should only be called when has_server_nonce is true.
    // Returns the next server_nonce specified by the server and removes it
    // from the queue of nonces.
    QuicString GetNextServerNonce();

    // SetProofVerifyDetails takes ownership of |details|.
    void SetProofVerifyDetails(ProofVerifyDetails* details);

    // Copy the |server_config_|, |source_address_token_|, |certs_|,
    // |expiration_time_|, |cert_sct_|, |chlo_hash_| and |server_config_sig_|
    // from the |other|.  The remaining fields, |generation_counter_|,
    // |proof_verify_details_|, and |scfg_| remain unchanged.
    void InitializeFrom(const CachedState& other);

    // Initializes this cached state based on the arguments provided.
    // Returns false if there is a problem parsing the server config.
    bool Initialize(QuicStringPiece server_config,
                    QuicStringPiece source_address_token,
                    const std::vector<QuicString>& certs,
                    const QuicString& cert_sct,
                    QuicStringPiece chlo_hash,
                    QuicStringPiece signature,
                    QuicWallTime now,
                    QuicWallTime expiration_time);

   private:
    QuicString server_config_;         // A serialized handshake message.
    QuicString source_address_token_;  // An opaque proof of IP ownership.
    std::vector<QuicString> certs_;    // A list of certificates in leaf-first
                                       // order.
    QuicString cert_sct_;              // Signed timestamp of the leaf cert.
    QuicString chlo_hash_;             // Hash of the CHLO message.
    QuicString server_config_sig_;     // A signature of |server_config_|.
    bool server_config_valid_;         // True if |server_config_| is correctly
                                // signed and |certs_| has been validated.
    QuicWallTime expiration_time_;  // Time when the config is no longer valid.
    // Generation counter associated with the |server_config_|, |certs_| and
    // |server_config_sig_| combination. It is incremented whenever we set
    // server_config_valid_ to false.
    uint64_t generation_counter_;

    std::unique_ptr<ProofVerifyDetails> proof_verify_details_;

    // scfg contains the cached, parsed value of |server_config|.
    mutable std::unique_ptr<CryptoHandshakeMessage> scfg_;

    // TODO(jokulik): Consider using a hash-set as extra book-keeping to ensure
    // that no connection-id is added twice.  Also, consider keeping the server
    // nonces and connection_ids together in one queue.
    QuicQueue<QuicConnectionId> server_designated_connection_ids_;
    QuicQueue<QuicString> server_nonces_;
  };

  // Used to filter server ids for partial config deletion.
  class ServerIdFilter {
   public:
    virtual ~ServerIdFilter() {}

    // Returns true if |server_id| matches the filter.
    virtual bool Matches(const QuicServerId& server_id) const = 0;
  };

  QuicCryptoClientConfig(std::unique_ptr<ProofVerifier> proof_verifier,
                         bssl::UniquePtr<SSL_CTX> ssl_ctx);
  QuicCryptoClientConfig(const QuicCryptoClientConfig&) = delete;
  QuicCryptoClientConfig& operator=(const QuicCryptoClientConfig&) = delete;
  ~QuicCryptoClientConfig();

  // LookupOrCreate returns a CachedState for the given |server_id|. If no such
  // CachedState currently exists, it will be created and cached.
  CachedState* LookupOrCreate(const QuicServerId& server_id);

  // Delete CachedState objects whose server ids match |filter| from
  // cached_states.
  void ClearCachedStates(const ServerIdFilter& filter);

  // FillInchoateClientHello sets |out| to be a CHLO message that elicits a
  // source-address token or SCFG from a server. If |cached| is non-nullptr, the
  // source-address token will be taken from it. |out_params| is used in order
  // to store the cached certs that were sent as hints to the server in
  // |out_params->cached_certs|. |preferred_version| is the version of the
  // QUIC protocol that this client chose to use initially. This allows the
  // server to detect downgrade attacks.  If |demand_x509_proof| is true,
  // then |out| will include an X509 proof demand, and the associated
  // certificate related fields.
  void FillInchoateClientHello(
      const QuicServerId& server_id,
      const ParsedQuicVersion preferred_version,
      const CachedState* cached,
      QuicRandom* rand,
      bool demand_x509_proof,
      QuicReferenceCountedPointer<QuicCryptoNegotiatedParameters> out_params,
      CryptoHandshakeMessage* out) const;

  // FillClientHello sets |out| to be a CHLO message based on the configuration
  // of this object. This object must have cached enough information about
  // the server's hostname in order to perform a handshake. This can be checked
  // with the |IsComplete| member of |CachedState|.
  //
  // |now| and |rand| are used to generate the nonce and |out_params| is
  // filled with the results of the handshake that the server is expected to
  // accept. |preferred_version| is the version of the QUIC protocol that this
  // client chose to use initially. This allows the server to detect downgrade
  // attacks.
  //
  // If |channel_id_key| is not null, it is used to sign a secret value derived
  // from the client and server's keys, and the Channel ID public key and the
  // signature are placed in the CETV value of the CHLO.
  QuicErrorCode FillClientHello(
      const QuicServerId& server_id,
      QuicConnectionId connection_id,
      const ParsedQuicVersion preferred_version,
      const CachedState* cached,
      QuicWallTime now,
      QuicRandom* rand,
      const ChannelIDKey* channel_id_key,
      QuicReferenceCountedPointer<QuicCryptoNegotiatedParameters> out_params,
      CryptoHandshakeMessage* out,
      QuicString* error_details) const;

  // ProcessRejection processes a REJ message from a server and updates the
  // cached information about that server. After this, |IsComplete| may return
  // true for that server's CachedState. If the rejection message contains state
  // about a future handshake (i.e. an nonce value from the server), then it
  // will be saved in |out_params|. |now| is used to judge whether the server
  // config in the rejection message has expired.
  QuicErrorCode ProcessRejection(
      const CryptoHandshakeMessage& rej,
      QuicWallTime now,
      QuicTransportVersion version,
      QuicStringPiece chlo_hash,
      CachedState* cached,
      QuicReferenceCountedPointer<QuicCryptoNegotiatedParameters> out_params,
      QuicString* error_details);

  // ProcessServerHello processes the message in |server_hello|, updates the
  // cached information about that server, writes the negotiated parameters to
  // |out_params| and returns QUIC_NO_ERROR. If |server_hello| is unacceptable
  // then it puts an error message in |error_details| and returns an error
  // code. |version| is the QUIC version for the current connection.
  // |negotiated_versions| contains the list of version, if any, that were
  // present in a version negotiation packet previously recevied from the
  // server. The contents of this list will be compared against the list of
  // versions provided in the VER tag of the server hello.
  QuicErrorCode ProcessServerHello(
      const CryptoHandshakeMessage& server_hello,
      QuicConnectionId connection_id,
      ParsedQuicVersion version,
      const ParsedQuicVersionVector& negotiated_versions,
      CachedState* cached,
      QuicReferenceCountedPointer<QuicCryptoNegotiatedParameters> out_params,
      QuicString* error_details);

  // Processes the message in |server_update|, updating the cached source
  // address token, and server config.
  // If |server_update| is invalid then |error_details| will contain an error
  // message, and an error code will be returned. If all has gone well
  // QUIC_NO_ERROR is returned.
  QuicErrorCode ProcessServerConfigUpdate(
      const CryptoHandshakeMessage& server_update,
      QuicWallTime now,
      const QuicTransportVersion version,
      QuicStringPiece chlo_hash,
      CachedState* cached,
      QuicReferenceCountedPointer<QuicCryptoNegotiatedParameters> out_params,
      QuicString* error_details);

  ProofVerifier* proof_verifier() const;

  ChannelIDSource* channel_id_source() const;

  SSL_CTX* ssl_ctx() const;

  // SetChannelIDSource sets a ChannelIDSource that will be called, when the
  // server supports channel IDs, to obtain a channel ID for signing a message
  // proving possession of the channel ID. This object takes ownership of
  // |source|.
  void SetChannelIDSource(ChannelIDSource* source);

  // Initialize the CachedState from |canonical_crypto_config| for the
  // |canonical_server_id| as the initial CachedState for |server_id|. We will
  // copy config data only if |canonical_crypto_config| has valid proof.
  void InitializeFrom(const QuicServerId& server_id,
                      const QuicServerId& canonical_server_id,
                      QuicCryptoClientConfig* canonical_crypto_config);

  // Adds |suffix| as a domain suffix for which the server's crypto config
  // is expected to be shared among servers with the domain suffix. If a server
  // matches this suffix, then the server config from another server with the
  // suffix will be used to initialize the cached state for this server.
  void AddCanonicalSuffix(const QuicString& suffix);

  // Saves the |user_agent_id| that will be passed in QUIC's CHLO message.
  void set_user_agent_id(const QuicString& user_agent_id) {
    user_agent_id_ = user_agent_id;
  }

  // Returns the user_agent_id that will be provided in the client hello
  // handshake message.
  const QuicString& user_agent_id() const { return user_agent_id_; }

  // Saves the |alpn| that will be passed in QUIC's CHLO message.
  void set_alpn(const QuicString& alpn) { alpn_ = alpn; }

  void set_pre_shared_key(QuicStringPiece psk) {
    pre_shared_key_ = QuicString(psk);
  }

 private:
  // Sets the members to reasonable, default values.
  void SetDefaults();

  // CacheNewServerConfig checks for SCFG, STK, PROF, and CRT tags in |message|,
  // verifies them, and stores them in the cached state if they validate.
  // This is used on receipt of a REJ from a server, or when a server sends
  // updated server config during a connection.
  QuicErrorCode CacheNewServerConfig(
      const CryptoHandshakeMessage& message,
      QuicWallTime now,
      QuicTransportVersion version,
      QuicStringPiece chlo_hash,
      const std::vector<QuicString>& cached_certs,
      CachedState* cached,
      QuicString* error_details);

  // If the suffix of the hostname in |server_id| is in |canonical_suffixes_|,
  // then populate |cached| with the canonical cached state from
  // |canonical_server_map_| for that suffix. Returns true if |cached| is
  // initialized with canonical cached state.
  bool PopulateFromCanonicalConfig(const QuicServerId& server_id,
                                   CachedState* cached);

  // cached_states_ maps from the server_id to the cached information about
  // that server.
  std::map<QuicServerId, std::unique_ptr<CachedState>> cached_states_;

  // Contains a map of servers which could share the same server config. Map
  // from a canonical host suffix/port/scheme to a representative server with
  // the canonical suffix, which has a plausible set of initial certificates
  // (or at least server public key).
  std::map<QuicServerId, QuicServerId> canonical_server_map_;

  // Contains list of suffixes (for exmaple ".c.youtube.com",
  // ".googlevideo.com") of canonical hostnames.
  std::vector<QuicString> canonical_suffixes_;

  std::unique_ptr<ProofVerifier> proof_verifier_;
  std::unique_ptr<ChannelIDSource> channel_id_source_;
  bssl::UniquePtr<SSL_CTX> ssl_ctx_;

  // The |user_agent_id_| passed in QUIC's CHLO message.
  QuicString user_agent_id_;

  // The |alpn_| passed in QUIC's CHLO message.
  QuicString alpn_;

  // If non-empty, the client will operate in the pre-shared key mode by
  // incorporating |pre_shared_key_| into the key schedule.
  QuicString pre_shared_key_;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_CRYPTO_QUIC_CRYPTO_CLIENT_CONFIG_H_
