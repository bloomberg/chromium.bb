// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"

#include "base/hash/sha1.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "net/cert/x509_util.h"
#include "url/gurl.h"

namespace safe_browsing {

namespace {

// Escapes a certificate attribute so that it can be used in a allowlist
// entry.  Currently, we only escape slashes, since they are used as a
// separator between attributes.
std::string EscapeCertAttribute(const std::string& attribute) {
  std::string escaped;
  for (size_t i = 0; i < attribute.size(); ++i) {
    if (attribute[i] == '%') {
      escaped.append("%25");
    } else if (attribute[i] == '/') {
      escaped.append("%2F");
    } else {
      escaped.push_back(attribute[i]);
    }
  }
  return escaped;
}

}  // namespace

void GetCertificateAllowlistStrings(
    const net::X509Certificate& certificate,
    const net::X509Certificate& issuer,
    std::vector<std::string>* allowlist_strings) {
  // The allowlist paths are in the format:
  // cert/<ascii issuer fingerprint>[/CN=common_name][/O=org][/OU=unit]
  //
  // Any of CN, O, or OU may be omitted from the allowlist entry, in which
  // case they match anything.  However, the attributes that do appear will
  // always be in the order shown above.  At least one attribute will always
  // be present.

  const net::CertPrincipal& subject = certificate.subject();
  std::vector<std::string> ou_tokens;
  for (size_t i = 0; i < subject.organization_unit_names.size(); ++i) {
    ou_tokens.push_back(
        "/OU=" + EscapeCertAttribute(subject.organization_unit_names[i]));
  }

  std::vector<std::string> o_tokens;
  for (size_t i = 0; i < subject.organization_names.size(); ++i) {
    o_tokens.push_back("/O=" +
                       EscapeCertAttribute(subject.organization_names[i]));
  }

  std::string cn_token;
  if (!subject.common_name.empty()) {
    cn_token = "/CN=" + EscapeCertAttribute(subject.common_name);
  }

  std::set<std::string> paths_to_check;
  if (!cn_token.empty()) {
    paths_to_check.insert(cn_token);
  }
  for (size_t i = 0; i < o_tokens.size(); ++i) {
    paths_to_check.insert(cn_token + o_tokens[i]);
    paths_to_check.insert(o_tokens[i]);
    for (size_t j = 0; j < ou_tokens.size(); ++j) {
      paths_to_check.insert(cn_token + o_tokens[i] + ou_tokens[j]);
      paths_to_check.insert(o_tokens[i] + ou_tokens[j]);
    }
  }
  for (size_t i = 0; i < ou_tokens.size(); ++i) {
    paths_to_check.insert(cn_token + ou_tokens[i]);
    paths_to_check.insert(ou_tokens[i]);
  }

  std::string hashed = base::SHA1HashString(std::string(
      net::x509_util::CryptoBufferAsStringPiece(issuer.cert_buffer())));
  std::string issuer_fp = base::HexEncode(hashed.data(), hashed.size());
  for (auto it = paths_to_check.begin(); it != paths_to_check.end(); ++it) {
    allowlist_strings->push_back("cert/" + issuer_fp + *it);
  }
}

GURL GetFileSystemAccessDownloadUrl(const GURL& frame_url) {
  // Regular blob: URLs are of the form
  // "blob:https://my-origin.com/def07373-cbd8-49d2-9ef7-20b071d34a1a". To make
  // these URLs distinguishable from those we use a fixed string rather than a
  // random UUID.
  return GURL("blob:" + frame_url.DeprecatedGetOriginAsURL().spec() +
              "file-system-access-write");
}

ClientDownloadResponse::Verdict DownloadDangerTypeToDownloadResponseVerdict(
    download::DownloadDangerType download_danger_type) {
  switch (download_danger_type) {
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
      return ClientDownloadResponse::DANGEROUS;
    case download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
      return ClientDownloadResponse::UNCOMMON;
    case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
      return ClientDownloadResponse::POTENTIALLY_UNWANTED;
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
      return ClientDownloadResponse::DANGEROUS_HOST;
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE:
      return ClientDownloadResponse::DANGEROUS_ACCOUNT_COMPROMISE;
    default:
      // Return SAFE for any other danger types.
      return ClientDownloadResponse::SAFE;
  }
}

}  // namespace safe_browsing
