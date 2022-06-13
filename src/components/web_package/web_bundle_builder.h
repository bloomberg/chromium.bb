// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_WEB_BUNDLE_BUILDER_H_
#define COMPONENTS_WEB_PACKAGE_WEB_BUNDLE_BUILDER_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/string_piece.h"
#include "components/cbor/writer.h"

namespace web_package {

enum class BundleVersion {
  kB1,
  kB2,
};

// This class can be used to create a Web Bundle.
class WebBundleBuilder {
 public:
  using Headers = std::vector<std::pair<std::string, std::string>>;
  struct ResponseLocation {
    // /components/cbor uses int64_t for integer types.
    int64_t offset;
    int64_t length;
  };

  WebBundleBuilder(const std::string& fallback_url,
                   const std::string& manifest_url,
                   BundleVersion version = BundleVersion::kB2,
                   bool allow_invalid_utf8_strings_for_testing = false);

  ~WebBundleBuilder();

  void AddExchange(base::StringPiece url,
                   const Headers& response_headers,
                   base::StringPiece payload);

  ResponseLocation AddResponse(const Headers& headers,
                               base::StringPiece payload);

  void AddIndexEntry(base::StringPiece url,
                     base::StringPiece variants_value,
                     std::vector<ResponseLocation> response_locations);
  void AddSection(base::StringPiece name, cbor::Value section);
  void AddAuthority(cbor::Value::MapValue authority);
  void AddVouchedSubset(cbor::Value::MapValue vouched_subset);

  std::vector<uint8_t> CreateBundle();

  // Creates a signed-subset structure with single subset-hashes entry,
  // and returns it as a CBOR bytestring.
  cbor::Value CreateEncodedSigned(base::StringPiece validity_url,
                                  base::StringPiece auth_sha256,
                                  int64_t date,
                                  int64_t expires,
                                  base::StringPiece url,
                                  base::StringPiece header_sha256,
                                  base::StringPiece payload_integrity_header);

 private:
  std::vector<uint8_t> CreateTopLevel();
  std::vector<uint8_t> Encode(const cbor::Value& value);
  cbor::Value GetCborValueOfURL(base::StringPiece url);

  int64_t EncodedLength(const cbor::Value& value);

  cbor::Writer::Config writer_config_;
  std::string fallback_url_;
  cbor::Value::ArrayValue section_lengths_;
  cbor::Value::ArrayValue sections_;
  std::map<std::string, std::pair<std::string, std::vector<ResponseLocation>>>
      delayed_index_;
  cbor::Value::ArrayValue responses_;
  cbor::Value::ArrayValue authorities_;
  cbor::Value::ArrayValue vouched_subsets_;
  BundleVersion version_;
  int64_t current_responses_offset_ = 0;
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_WEB_BUNDLE_BUILDER_H_
