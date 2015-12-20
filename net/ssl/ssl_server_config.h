// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_SSL_SERVER_CONFIG_H_
#define NET_SSL_SSL_SERVER_CONFIG_H_

#include <stdint.h>

#include <vector>

#include "net/base/net_export.h"
#include "net/ssl/ssl_config.h"

namespace net {

// A collection of server-side SSL-related configuration settings.
struct NET_EXPORT SSLServerConfig {
  // Defaults
  SSLServerConfig();
  ~SSLServerConfig();

  // The minimum and maximum protocol versions that are enabled.
  // (Use the SSL_PROTOCOL_VERSION_xxx enumerators defined in ssl_config.h)
  // SSL 2.0 and SSL 3.0 are not supported. If version_max < version_min, it
  // means no protocol versions are enabled.
  uint16_t version_min;
  uint16_t version_max;

  // Presorted list of cipher suites which should be explicitly prevented from
  // being used in addition to those disabled by the net built-in policy.
  //
  // By default, all cipher suites supported by the underlying SSL
  // implementation will be enabled except for:
  // - Null encryption cipher suites.
  // - Weak cipher suites: < 80 bits of security strength.
  // - FORTEZZA cipher suites (obsolete).
  // - IDEA cipher suites (RFC 5469 explains why).
  // - Anonymous cipher suites.
  // - ECDSA cipher suites on platforms that do not support ECDSA signed
  //   certificates, as servers may use the presence of such ciphersuites as a
  //   hint to send an ECDSA certificate.
  // The ciphers listed in |disabled_cipher_suites| will be removed in addition
  // to the above list.
  //
  // Though cipher suites are sent in TLS as "uint8_t CipherSuite[2]", in
  // big-endian form, they should be declared in host byte order, with the
  // first uint8_t occupying the most significant byte.
  // Ex: To disable TLS_RSA_WITH_RC4_128_MD5, specify 0x0004, while to
  // disable TLS_ECDH_ECDSA_WITH_RC4_128_SHA, specify 0xC002.
  std::vector<uint16_t> disabled_cipher_suites;

  // If true, causes only ECDHE cipher suites to be enabled.
  bool require_ecdhe;

  // Requires a client certificate for client authentication from the client.
  // This doesn't currently enforce certificate validity.
  bool require_client_cert;
};

}  // namespace net

#endif  // NET_SSL_SSL_SERVER_CONFIG_H_
