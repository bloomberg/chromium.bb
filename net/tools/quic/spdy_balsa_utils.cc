// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/spdy_balsa_utils.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "net/quic/quic_flags.h"
#include "net/quic/spdy_utils.h"
#include "net/spdy/spdy_frame_builder.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_protocol.h"
#include "net/tools/balsa/balsa_headers.h"
#include "url/gurl.h"

using base::StringPiece;
using std::make_pair;
using std::pair;
using std::string;

namespace net {
namespace tools {
namespace {

const char kV4Host[] = ":authority";

const char kV3Host[] = ":host";
const char kV3Path[] = ":path";
const char kV3Scheme[] = ":scheme";
const char kV3Method[] = ":method";
const char kV3Status[] = ":status";
const char kV3Version[] = ":version";

void PopulateSpdyHeaderBlock(const BalsaHeaders& headers,
                             SpdyHeaderBlock* block,
                             bool allow_empty_values) {
  for (BalsaHeaders::const_header_lines_iterator hi =
           headers.header_lines_begin();
       hi != headers.header_lines_end(); ++hi) {
    if ((hi->second.length() == 0) && !allow_empty_values) {
      DVLOG(1) << "Dropping empty header " << hi->first.as_string()
               << " from headers";
      continue;
    }

    // This unfortunately involves loads of copying, but its the simplest way
    // to sort the headers and leverage the framer.
    string name = base::ToLowerASCII(hi->first.as_string());
    SpdyHeaderBlock::iterator it = block->find(name);
    if (it != block->end()) {
      it->second.reserve(it->second.size() + 1 + hi->second.size());
      it->second.append("\0", 1);
      it->second.append(hi->second.data(), hi->second.size());
    } else {
      block->insert(make_pair(name, hi->second.as_string()));
    }
  }
}

void PopulateSpdy3RequestHeaderBlock(const BalsaHeaders& headers,
                                     const string& scheme,
                                     const string& host_and_port,
                                     const string& path,
                                     SpdyHeaderBlock* block) {
  PopulateSpdyHeaderBlock(headers, block, true);
  StringPiece host_header = headers.GetHeader("Host");
  if (!host_header.empty()) {
    DCHECK(host_and_port.empty() || host_header == host_and_port);
    block->insert(make_pair(kV3Host, host_header.as_string()));
  } else {
    block->insert(make_pair(kV3Host, host_and_port));
  }
  block->insert(make_pair(kV3Path, path));
  block->insert(make_pair(kV3Scheme, scheme));

  if (!headers.request_method().empty()) {
    block->insert(make_pair(kV3Method, headers.request_method().as_string()));
  }

  if (!headers.request_version().empty()) {
    (*block)[kV3Version] = headers.request_version().as_string();
  }
}

void PopulateSpdy4RequestHeaderBlock(const BalsaHeaders& headers,
                                     const string& scheme,
                                     const string& host_and_port,
                                     const string& path,
                                     SpdyHeaderBlock* block) {
  PopulateSpdyHeaderBlock(headers, block, true);
  StringPiece host_header = headers.GetHeader("Host");
  if (!host_header.empty()) {
    DCHECK(host_and_port.empty() || host_header == host_and_port);
    block->insert(make_pair(kV4Host, host_header.as_string()));
    // PopulateSpdyHeaderBlock already added the "host" header,
    // which is invalid for SPDY4.
    block->erase("host");
  } else {
    block->insert(make_pair(kV4Host, host_and_port));
  }
  block->insert(make_pair(kV3Path, path));
  block->insert(make_pair(kV3Scheme, scheme));

  if (!headers.request_method().empty()) {
    block->insert(make_pair(kV3Method, headers.request_method().as_string()));
  }
}

void PopulateSpdyResponseHeaderBlock(SpdyMajorVersion version,
                                     const BalsaHeaders& headers,
                                     SpdyHeaderBlock* block) {
  if (version <= SPDY3) {
    string status = headers.response_code().as_string();
    status.append(" ");
    status.append(headers.response_reason_phrase().as_string());
    (*block)[kV3Status] = status;
    (*block)[kV3Version] = headers.response_version().as_string();
  } else {
    (*block)[kV3Status] = headers.response_code().as_string();
  }

  PopulateSpdyHeaderBlock(headers, block, true);
}

bool IsSpecialSpdyHeader(SpdyHeaderBlock::const_iterator header,
                         BalsaHeaders* headers) {
  if (header->first.empty() || header->second.empty()) {
    return true;
  }
  const string& header_name = header->first;
  return header_name.c_str()[0] == ':';
}

// The reason phrase should match regexp [\d\d\d [^\r\n]+].  If not, we will
// fail to parse it.
bool ParseReasonAndStatus(StringPiece status_and_reason,
                          BalsaHeaders* headers,
                          QuicVersion quic_version) {
  if (quic_version > QUIC_VERSION_24) {
    int status;
    if (!base::StringToInt(status_and_reason, &status)) {
      return false;
    }
    headers->SetResponseCode(status_and_reason);
    headers->SetResponseCode(status_and_reason);
    headers->set_parsed_response_code(status);
    return true;
  }

  if (status_and_reason.size() < 5)
    return false;

  if (status_and_reason[3] != ' ')
    return false;

  const StringPiece status_str = StringPiece(status_and_reason.data(), 3);
  int status;
  if (!base::StringToInt(status_str, &status)) {
    return false;
  }

  headers->SetResponseCode(status_str);
  headers->set_parsed_response_code(status);

  StringPiece reason(status_and_reason.data() + 4,
                     status_and_reason.length() - 4);

  headers->SetResponseReasonPhrase(reason);
  return true;
}

// static
void SpdyHeadersToResponseHeaders(const SpdyHeaderBlock& header_block,
                                  BalsaHeaders* request_headers,
                                  QuicVersion quic_version) {
  typedef SpdyHeaderBlock::const_iterator BlockIt;

  BlockIt status_it = header_block.find(kV3Status);
  BlockIt version_it = header_block.find(kV3Version);
  BlockIt end_it = header_block.end();
  if (quic_version > QUIC_VERSION_24) {
    if (status_it == end_it) {
      return;
    }
  } else {
    if (status_it == end_it || version_it == end_it) {
      return;
    }
  }

  if (!ParseReasonAndStatus(status_it->second, request_headers, quic_version)) {
    return;
  }

  if (quic_version <= QUIC_VERSION_24) {
    request_headers->SetResponseVersion(version_it->second);
  }
  for (BlockIt it = header_block.begin(); it != header_block.end(); ++it) {
    if (!IsSpecialSpdyHeader(it, request_headers)) {
      request_headers->AppendHeader(it->first, it->second);
    }
  }
}

// static
void SpdyHeadersToRequestHeaders(const SpdyHeaderBlock& header_block,
                                 BalsaHeaders* request_headers,
                                 QuicVersion quic_version) {
  typedef SpdyHeaderBlock::const_iterator BlockIt;

  BlockIt authority_it = header_block.find(kV4Host);
  BlockIt host_it = header_block.find(kV3Host);
  BlockIt method_it = header_block.find(kV3Method);
  BlockIt path_it = header_block.find(kV3Path);
  BlockIt scheme_it = header_block.find(kV3Scheme);
  BlockIt end_it = header_block.end();

  string method;
  if (method_it == end_it) {
    method = "GET";
  } else {
    method = method_it->second;
  }
  string uri;
  if (path_it == end_it) {
    uri = "/";
  } else {
    uri = path_it->second;
  }
  request_headers->SetRequestFirstlineFromStringPieces(
      method, uri, net::kHttp2VersionString);

  if (scheme_it == end_it) {
    request_headers->AppendHeader("Scheme", "https");
  } else {
    request_headers->AppendHeader("Scheme", scheme_it->second);
  }
  if (authority_it != end_it) {
    request_headers->AppendHeader("host", authority_it->second);
  } else if (host_it != end_it) {
    request_headers->AppendHeader("host", host_it->second);
  }

  for (BlockIt it = header_block.begin(); it != header_block.end(); ++it) {
    if (!IsSpecialSpdyHeader(it, request_headers)) {
      request_headers->AppendHeader(it->first, it->second);
    }
  }
}

// static
void SpdyHeadersToBalsaHeaders(const SpdyHeaderBlock& block,
                               BalsaHeaders* headers,
                               QuicVersion quic_version,
                               SpdyHeaderValidatorType type) {
  if (type == SpdyHeaderValidatorType::RESPONSE_HEADER) {
    SpdyHeadersToResponseHeaders(block, headers, quic_version);
    return;
  }
  SpdyHeadersToRequestHeaders(block, headers, quic_version);
}

}  // namespace

// static
SpdyHeaderBlock SpdyBalsaUtils::RequestHeadersToSpdyHeaders(
    const BalsaHeaders& request_headers,
    QuicVersion quic_version) {
  string scheme;
  string host_and_port;
  string path;

  string url = request_headers.request_uri().as_string();
  if (url.empty() || url[0] == '/') {
    path = url;
  } else {
    GURL request_uri(url);
    if (request_headers.request_method() == "CONNECT") {
      path = url;
    } else {
      path = request_uri.path();
      if (!request_uri.query().empty()) {
        path = path + "?" + request_uri.query();
      }
      host_and_port = request_uri.host();
      scheme = request_uri.scheme();
    }
  }

  DCHECK(!scheme.empty());
  DCHECK(!host_and_port.empty());
  DCHECK(!path.empty());

  SpdyHeaderBlock block;
  if (net::SpdyUtils::GetSpdyVersionForQuicVersion(quic_version) == SPDY3) {
    PopulateSpdy3RequestHeaderBlock(request_headers, scheme, host_and_port,
                                    path, &block);
  } else {
    PopulateSpdy4RequestHeaderBlock(request_headers, scheme, host_and_port,
                                    path, &block);
  }
  if (!FLAGS_spdy_strip_invalid_headers) {
    if (block.find("host") != block.end()) {
      block.erase(block.find("host"));
    }
    if (block.find("connection") != block.end()) {
      block.erase(block.find("connection"));
    }
  }
  return block;
}

// static
SpdyHeaderBlock SpdyBalsaUtils::ResponseHeadersToSpdyHeaders(
    const BalsaHeaders& response_headers,
    QuicVersion quic_version) {
  SpdyHeaderBlock block;
  PopulateSpdyResponseHeaderBlock(
      net::SpdyUtils::GetSpdyVersionForQuicVersion(quic_version),
      response_headers, &block);
  return block;
}

// static
string SpdyBalsaUtils::SerializeResponseHeaders(
    const BalsaHeaders& response_headers,
    QuicVersion quic_version) {
  SpdyHeaderBlock block =
      ResponseHeadersToSpdyHeaders(response_headers, quic_version);

  return net::SpdyUtils::SerializeUncompressedHeaders(block, quic_version);
}

// static
void SpdyBalsaUtils::SpdyHeadersToResponseHeaders(const SpdyHeaderBlock& block,
                                                  BalsaHeaders* headers,
                                                  QuicVersion quic_version) {
  SpdyHeadersToBalsaHeaders(block, headers, quic_version,
                            SpdyHeaderValidatorType::RESPONSE_HEADER);
}

// static
void SpdyBalsaUtils::SpdyHeadersToRequestHeaders(const SpdyHeaderBlock& block,
                                                 BalsaHeaders* headers,
                                                 QuicVersion quic_version) {
  SpdyHeadersToBalsaHeaders(block, headers, quic_version,
                            SpdyHeaderValidatorType::REQUEST);
}

}  // namespace tools
}  // namespace net
