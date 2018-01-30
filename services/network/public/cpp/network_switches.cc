// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/network_switches.h"

namespace network {

namespace switches {

// These mappings only apply to the host resolver.
const char kHostResolverRules[] = "host-resolver-rules";

// A set of public key hashes for which to ignore certificate-related errors.
//
// If the certificate chain presented by the server does not validate, and one
// or more certificates have public key hashes that match a key from this list,
// the error is ignored.
//
// The switch value must a be a comma-separated list of Base64-encoded SHA-256
// SPKI Fingerprints (RFC 7469, Section 2.4).
//
// This switch has no effect unless --user-data-dir (as defined by the content
// embedder) is also present.
const char kIgnoreCertificateErrorsSPKIList[] =
    "ignore-certificate-errors-spki-list";

// Enables saving net log events to a file. If a value is given, it used as the
// path the the file, otherwise the file is named netlog.json and placed in the
// user data directory.
const char kLogNetLog[] = "log-net-log";

// Don't send HTTP-Referer headers.
const char kNoReferrers[] = "no-referrers";

}  // namespace switches

}  // namespace network
