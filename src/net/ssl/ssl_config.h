// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_SSL_CONFIG_H_
#define NET_SSL_SSL_CONFIG_H_

#include <stdint.h>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "net/base/net_export.h"
#include "net/base/network_isolation_key.h"
#include "net/base/privacy_mode.h"
#include "net/cert/x509_certificate.h"
#include "net/socket/next_proto.h"
#include "net/ssl/ssl_private_key.h"

namespace net {

// Various TLS/SSL ProtocolVersion values encoded as uint16_t
//      struct {
//          uint8_t major;
//          uint8_t minor;
//      } ProtocolVersion;
// The most significant byte is |major|, and the least significant byte
// is |minor|.
enum {
  SSL_PROTOCOL_VERSION_TLS1 = 0x0301,
  SSL_PROTOCOL_VERSION_TLS1_1 = 0x0302,
  SSL_PROTOCOL_VERSION_TLS1_2 = 0x0303,
  SSL_PROTOCOL_VERSION_TLS1_3 = 0x0304,
};

// Default minimum protocol version.
NET_EXPORT extern const uint16_t kDefaultSSLVersionMin;

// Default minimum protocol version to warn about.
NET_EXPORT extern const uint16_t kDefaultSSLVersionMinWarn;

// Default maximum protocol version.
NET_EXPORT extern const uint16_t kDefaultSSLVersionMax;

// A collection of SSL-related configuration settings.
struct NET_EXPORT SSLConfig {
  // Default to revocation checking.
  SSLConfig();
  SSLConfig(const SSLConfig& other);
  ~SSLConfig();

  // Returns true if |cert| is one of the certs in |allowed_bad_certs|.
  // The expected cert status is written to |cert_status|. |*cert_status| can
  // be NULL if user doesn't care about the cert status.
  bool IsAllowedBadCert(X509Certificate* cert, CertStatus* cert_status) const;

  // Returns the set of flags to use for certificate verification, which is a
  // bitwise OR of CertVerifier::VerifyFlags that represent this SSLConfig's
  // configuration.
  int GetCertVerifyFlags() const;

  // If specified, the minimum and maximum protocol versions that are enabled.
  // (Use the SSL_PROTOCOL_VERSION_xxx enumerators defined above.) If
  // unspecified, values from the SSLConfigService are used.
  base::Optional<uint16_t> version_min_override;
  base::Optional<uint16_t> version_max_override;

  // Whether early data is enabled on this connection. Note that early data has
  // weaker security properties than normal data and changes the
  // SSLClientSocket's behavior. The caller must only send replayable data prior
  // to handshake confirmation. See StreamSocket::ConfirmHandshake for details.
  //
  // Additionally, early data may be rejected by the server, resulting in some
  // socket operation failing with ERR_EARLY_DATA_REJECTED or
  // ERR_WRONG_VERSION_ON_EARLY_DATA before any data is returned from the
  // server. The caller must handle these cases, typically by retrying the
  // high-level operation.
  //
  // If unsure, do not enable this option.
  bool early_data_enabled = false;

  // If true, causes only ECDHE cipher suites to be enabled.
  bool require_ecdhe = false;

  // If true, causes 3DES cipher suites and SHA-1 signature algorithms in
  // TLS 1.2 to be disabled.
  bool disable_legacy_crypto = false;

  // TODO(wtc): move the following members to a new SSLParams structure.  They
  // are not SSL configuration settings.

  struct NET_EXPORT CertAndStatus {
    CertAndStatus();
    CertAndStatus(scoped_refptr<X509Certificate> cert, CertStatus status);
    CertAndStatus(const CertAndStatus&);
    ~CertAndStatus();

    scoped_refptr<X509Certificate> cert;
    CertStatus cert_status = 0;
  };

  // Add any known-bad SSL certificate (with its cert status) to
  // |allowed_bad_certs| that should not trigger an ERR_CERT_* error when
  // calling SSLClientSocket::Connect.  This would normally be done in
  // response to the user explicitly accepting the bad certificate.
  std::vector<CertAndStatus> allowed_bad_certs;

  // True if all certificate errors should be ignored.
  bool ignore_certificate_errors = false;

  // True if, for a single connection, any dependent network fetches should
  // be disabled. This can be used to avoid triggering re-entrancy in the
  // network layer. For example, fetching a PAC script over HTTPS may cause
  // AIA, OCSP, or CRL fetches to block on retrieving the PAC script, while
  // the PAC script fetch is waiting for those dependent fetches, creating a
  // deadlock.
  bool disable_cert_verification_network_fetches = false;

  // The list of application level protocols supported with ALPN (Application
  // Layer Protocol Negotiation), in decreasing order of preference.  Protocols
  // will be advertised in this order during TLS handshake.
  NextProtoVector alpn_protos;

  // True if renegotiation should be allowed for the default application-level
  // protocol when the peer negotiates neither ALPN nor NPN.
  bool renego_allowed_default = false;

  // The list of application-level protocols to enable renegotiation for.
  NextProtoVector renego_allowed_for_protos;

  // If the PartitionSSLSessionsByNetworkIsolationKey feature is enabled, the
  // session cache is partitioned by this value.
  NetworkIsolationKey network_isolation_key;

  // An additional boolean to partition the session cache by.
  //
  // TODO(https://crbug.com/775438, https://crbug.com/951205): This should
  // additionally disable client certificates, once client certificate handling
  // is moved into SSLClientContext. With client certificates are disabled, the
  // current session cache partitioning behavior will be needed to correctly
  // implement it. For now, it acts as an incomplete version of
  // PartitionSSLSessionsByNetworkIsolationKey.
  PrivacyMode privacy_mode = PRIVACY_MODE_DISABLED;

  // True if the post-handshake peeking of the transport should be skipped. This
  // logic ensures tickets are resolved early, but can interfere with some unit
  // tests.
  bool disable_post_handshake_peek_for_testing = false;
};

}  // namespace net

#endif  // NET_SSL_SSL_CONFIG_H_
