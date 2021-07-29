// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quic/core/http/spdy_utils.h"

#include <memory>
#include <string>
#include <vector>

#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "quic/platform/api/quic_flag_utils.h"
#include "quic/platform/api/quic_flags.h"
#include "quic/platform/api/quic_logging.h"
#include "common/quiche_text_utils.h"
#include "spdy/core/spdy_protocol.h"

using spdy::SpdyHeaderBlock;

namespace quic {

// static
bool SpdyUtils::ExtractContentLengthFromHeaders(int64_t* content_length,
                                                SpdyHeaderBlock* headers) {
  auto it = headers->find("content-length");
  if (it == headers->end()) {
    return false;
  } else {
    // Check whether multiple values are consistent.
    absl::string_view content_length_header = it->second;
    std::vector<absl::string_view> values =
        absl::StrSplit(content_length_header, '\0');
    for (const absl::string_view& value : values) {
      uint64_t new_value;
      if (!absl::SimpleAtoi(value, &new_value) ||
          !quiche::QuicheTextUtils::IsAllDigits(value)) {
        QUIC_DLOG(ERROR)
            << "Content length was either unparseable or negative.";
        return false;
      }
      if (*content_length < 0) {
        *content_length = new_value;
        continue;
      }
      if (new_value != static_cast<uint64_t>(*content_length)) {
        QUIC_DLOG(ERROR)
            << "Parsed content length " << new_value << " is "
            << "inconsistent with previously detected content length "
            << *content_length;
        return false;
      }
    }
    return true;
  }
}

bool SpdyUtils::CopyAndValidateHeaders(const QuicHeaderList& header_list,
                                       int64_t* content_length,
                                       SpdyHeaderBlock* headers) {
  for (const auto& p : header_list) {
    const std::string& name = p.first;
    if (name.empty()) {
      QUIC_DLOG(ERROR) << "Header name must not be empty.";
      return false;
    }

    if (quiche::QuicheTextUtils::ContainsUpperCase(name)) {
      QUIC_DLOG(ERROR) << "Malformed header: Header name " << name
                       << " contains upper-case characters.";
      return false;
    }

    headers->AppendValueOrAddHeader(name, p.second);
  }

  if (headers->contains("content-length") &&
      !ExtractContentLengthFromHeaders(content_length, headers)) {
    return false;
  }

  QUIC_DVLOG(1) << "Successfully parsed headers: " << headers->DebugString();
  return true;
}

bool SpdyUtils::CopyAndValidateTrailers(const QuicHeaderList& header_list,
                                        bool expect_final_byte_offset,
                                        size_t* final_byte_offset,
                                        SpdyHeaderBlock* trailers) {
  bool found_final_byte_offset = false;
  for (const auto& p : header_list) {
    const std::string& name = p.first;

    // Pull out the final offset pseudo header which indicates the number of
    // response body bytes expected.
    if (expect_final_byte_offset && !found_final_byte_offset &&
        name == kFinalOffsetHeaderKey &&
        absl::SimpleAtoi(p.second, final_byte_offset)) {
      found_final_byte_offset = true;
      continue;
    }

    if (name.empty() || name[0] == ':') {
      QUIC_DLOG(ERROR)
          << "Trailers must not be empty, and must not contain pseudo-"
          << "headers. Found: '" << name << "'";
      return false;
    }

    if (quiche::QuicheTextUtils::ContainsUpperCase(name)) {
      QUIC_DLOG(ERROR) << "Malformed header: Header name " << name
                       << " contains upper-case characters.";
      return false;
    }

    trailers->AppendValueOrAddHeader(name, p.second);
  }

  if (expect_final_byte_offset && !found_final_byte_offset) {
    QUIC_DLOG(ERROR) << "Required key '" << kFinalOffsetHeaderKey
                     << "' not present";
    return false;
  }

  // TODO(rjshade): Check for other forbidden keys, following the HTTP/2 spec.

  QUIC_DVLOG(1) << "Successfully parsed Trailers: " << trailers->DebugString();
  return true;
}

// static
// TODO(danzh): Move it to quic/tools/ and switch to use GURL.
bool SpdyUtils::PopulateHeaderBlockFromUrl(const std::string url,
                                           SpdyHeaderBlock* headers) {
  (*headers)[":method"] = "GET";
  size_t pos = url.find("://");
  if (pos == std::string::npos) {
    return false;
  }
  (*headers)[":scheme"] = url.substr(0, pos);
  size_t start = pos + 3;
  pos = url.find("/", start);
  if (pos == std::string::npos) {
    (*headers)[":authority"] = url.substr(start);
    (*headers)[":path"] = "/";
    return true;
  }
  (*headers)[":authority"] = url.substr(start, pos - start);
  (*headers)[":path"] = url.substr(pos);
  return true;
}

// static
absl::optional<QuicDatagramStreamId> SpdyUtils::ParseDatagramFlowIdHeader(
    const spdy::SpdyHeaderBlock& headers) {
  auto flow_id_pair = headers.find("datagram-flow-id");
  if (flow_id_pair == headers.end()) {
    return absl::nullopt;
  }
  std::vector<absl::string_view> flow_id_strings =
      absl::StrSplit(flow_id_pair->second, ',');
  absl::optional<QuicDatagramStreamId> first_named_flow_id;
  for (absl::string_view flow_id_string : flow_id_strings) {
    std::vector<absl::string_view> flow_id_components =
        absl::StrSplit(flow_id_string, ';');
    if (flow_id_components.empty()) {
      continue;
    }
    absl::string_view flow_id_value_string = flow_id_components[0];
    quiche::QuicheTextUtils::RemoveLeadingAndTrailingWhitespace(
        &flow_id_value_string);
    QuicDatagramStreamId flow_id;
    if (!absl::SimpleAtoi(flow_id_value_string, &flow_id)) {
      continue;
    }
    if (flow_id_components.size() == 1) {
      // This flow ID is unnamed, return this one.
      return flow_id;
    }
    // Otherwise this is a named flow ID.
    if (!first_named_flow_id.has_value()) {
      first_named_flow_id = flow_id;
    }
  }
  return first_named_flow_id;
}

// static
void SpdyUtils::AddDatagramFlowIdHeader(spdy::SpdyHeaderBlock* headers,
                                        QuicDatagramStreamId flow_id) {
  (*headers)["datagram-flow-id"] = absl::StrCat(flow_id);
}

}  // namespace quic
