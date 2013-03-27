// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/spdy_utils.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "net/spdy/spdy_frame_builder.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_protocol.h"
#include "net/tools/flip_server/balsa_headers.h"

using base::StringPiece;
using std::pair;
using std::string;

namespace net {

const char* const kV3Host = ":host";
const char* const kV3Path = ":path";
const char* const kV3Scheme = ":scheme";
const char* const kV3Status = ":status";
const char* const kV3Method = ":method";
const char* const kV3Version = ":version";

void PopulateSpdyHeaderBlock(const BalsaHeaders& headers,
                             SpdyHeaderBlock* block,
                             bool allow_empty_values) {
  for (BalsaHeaders::const_header_lines_iterator hi =
       headers.header_lines_begin();
       hi != headers.header_lines_end();
       ++hi) {
    if ((hi->second.length() == 0) && !allow_empty_values) {
      DLOG(INFO) << "Dropping empty header " << hi->first.as_string()
                 << " from headers";
      continue;
    }

    // This unfortunately involves loads of copying, but its the simplest way
    // to sort the headers and leverage the framer.
    string name = hi->first.as_string();
    StringToLowerASCII(&name);
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

void PopulateSpdyResponseHeaderBlock(const BalsaHeaders& headers,
                                     SpdyHeaderBlock* block) {
  string status = headers.response_code().as_string();
  status.append(" ");
  status.append(headers.response_reason_phrase().as_string());
  (*block)[kV3Status] = status;
  (*block)[kV3Version] =
      headers.response_version().as_string();

  // Empty header values are only allowed because this is spdy3.
  PopulateSpdyHeaderBlock(headers, block, true);
}

string SpdyUtils::SerializeRequestHeaders(
    const BalsaHeaders& request_headers) {
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
      path = request_uri.path() + "?" + request_uri.query();
      host_and_port = request_uri.port();
      scheme = request_uri.scheme();
    }
  }

  SpdyHeaderBlock block;
  PopulateSpdy3RequestHeaderBlock(
      request_headers, scheme, host_and_port, path, &block);
  if (block.find("host") != block.end()) {
    block.erase(block.find("host"));
  }

  int length = SpdyFramer::GetSerializedLength(kSpdyVersion3, &block);
  SpdyFrameBuilder builder(length);
  SpdyFramer::WriteHeaderBlock(&builder, kSpdyVersion3, &block);
  scoped_ptr<SpdyFrame> headers(builder.take());
  return string(headers->data(), length);
}

string SpdyUtils::SerializeResponseHeaders(
    const BalsaHeaders& response_headers) {
  SpdyHeaderBlock block;
  PopulateSpdyResponseHeaderBlock(response_headers, &block);

  int length = SpdyFramer::GetSerializedLength(kSpdyVersion3, &block);
  SpdyFrameBuilder builder(length);
  SpdyFramer::WriteHeaderBlock(&builder, kSpdyVersion3, &block);
  scoped_ptr<SpdyFrame> headers(builder.take());
  return string(headers->data(), length);
}

bool IsSpecialSpdyHeader(SpdyHeaderBlock::const_iterator header,
                            BalsaHeaders* headers) {
  if (header->first.empty() || header->second.empty()) {
    return true;
  }
  const string& header_name = header->first;
  return header_name.c_str()[0] == ':';
}

bool SpdyUtils::FillBalsaRequestHeaders(
    const SpdyHeaderBlock& header_block,
    BalsaHeaders* request_headers) {
  typedef SpdyHeaderBlock::const_iterator BlockIt;

  BlockIt host_it = header_block.find(kV3Host);
  BlockIt path_it = header_block.find(kV3Path);
  BlockIt scheme_it = header_block.find(kV3Scheme);
  BlockIt method_it = header_block.find(kV3Method);
  BlockIt end_it = header_block.end();
  if (host_it == end_it || path_it == end_it || scheme_it == end_it ||
      method_it == end_it) {
    return false;
  }
  string url = scheme_it->second;
  url.append("://");
  url.append(host_it->second);
  url.append(path_it->second);
  request_headers->SetRequestUri(url);
  request_headers->SetRequestMethod(method_it->second);

  for (BlockIt it = header_block.begin(); it != header_block.end(); ++it) {
   if (!IsSpecialSpdyHeader(it, request_headers)) {
     request_headers->AppendHeader(it->first, it->second);
   }
  }
  return true;
}

// The reason phrase should match regexp [\d\d\d [^\r\n]+].  If not, we will
// fail to parse it.
bool ParseReasonAndStatus(StringPiece status_and_reason,
                          BalsaHeaders* headers) {
  if (status_and_reason.size() < 5)
    return false;

  if (status_and_reason[3] != ' ')
    return false;

  const StringPiece status = StringPiece(status_and_reason.data(), 3);
//  if (!IsDigitString(status))//FIXME
//    return false;

  headers->SetResponseCode(status);
  headers->set_parsed_response_code(atoi(status.data()));

  StringPiece reason(status_and_reason.data() + 4,
                     status_and_reason.length() - 4);

  headers->SetResponseReasonPhrase(reason);
  return true;
}

bool SpdyUtils::FillBalsaResponseHeaders(
    const SpdyHeaderBlock& header_block,
    BalsaHeaders* request_headers) {
  typedef SpdyHeaderBlock::const_iterator BlockIt;

  BlockIt status_it = header_block.find(kV3Status);
  BlockIt version_it = header_block.find(kV3Version);
  BlockIt end_it = header_block.end();
  if (status_it == end_it || version_it == end_it) {
    return false;
  }

  request_headers->SetRequestVersion(version_it->second);
  if (!ParseReasonAndStatus(status_it->second, request_headers)) {
    return false;
  }
  for (BlockIt it = header_block.begin(); it != header_block.end(); ++it) {
   if (!IsSpecialSpdyHeader(it, request_headers)) {
     request_headers->AppendHeader(it->first, it->second);
   }
  }
  return true;
}

}  // namespace net
