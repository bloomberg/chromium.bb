// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_in_memory_cache.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "net/tools/balsa/balsa_frame.h"
#include "net/tools/balsa/balsa_headers.h"
#include "net/tools/balsa/noop_balsa_visitor.h"
#include "net/tools/quic/spdy_utils.h"

using base::FilePath;
using base::StringPiece;
using std::string;

namespace net {
namespace tools {

namespace {

// BalsaVisitor implementation (glue) which caches response bodies.
class CachingBalsaVisitor : public NoOpBalsaVisitor {
 public:
  CachingBalsaVisitor() : done_framing_(false) {}
  void ProcessBodyData(const char* input, size_t size) override {
    AppendToBody(input, size);
  }
  void MessageDone() override { done_framing_ = true; }
  void HandleHeaderError(BalsaFrame* framer) override { UnhandledError(); }
  void HandleHeaderWarning(BalsaFrame* framer) override { UnhandledError(); }
  void HandleChunkingError(BalsaFrame* framer) override { UnhandledError(); }
  void HandleBodyError(BalsaFrame* framer) override { UnhandledError(); }
  void UnhandledError() {
    LOG(DFATAL) << "Unhandled error framing HTTP.";
  }
  void AppendToBody(const char* input, size_t size) {
    body_.append(input, size);
  }
  bool done_framing() const { return done_framing_; }
  const string& body() const { return body_; }

 private:
  bool done_framing_;
  string body_;
};

}  // namespace

QuicInMemoryCache::Response::Response() : response_type_(REGULAR_RESPONSE) {}

QuicInMemoryCache::Response::~Response() {}

// static
QuicInMemoryCache* QuicInMemoryCache::GetInstance() {
  return Singleton<QuicInMemoryCache>::get();
}

const QuicInMemoryCache::Response* QuicInMemoryCache::GetResponse(
    StringPiece host,
    StringPiece path) const {
  ResponseMap::const_iterator it = responses_.find(GetKey(host, path));
  if (it == responses_.end()) {
    return nullptr;
  }
  return it->second;
}

void QuicInMemoryCache::AddSimpleResponse(StringPiece host,
                                          StringPiece path,
                                          int response_code,
                                          StringPiece response_detail,
                                          StringPiece body) {
  SpdyHeaderBlock response_headers;
  response_headers[":version"] = "HTTP/1.1";
  string status = base::IntToString(response_code) + " ";
  response_detail.AppendToString(&status);
  response_headers[":status"] = status;
  response_headers["content-length"] = base::IntToString(body.length());
  AddResponse(host, path, response_headers, body);
}

void QuicInMemoryCache::AddResponse(StringPiece host,
                                    StringPiece path,
                                    const SpdyHeaderBlock& response_headers,
                                    StringPiece response_body) {
  AddResponseImpl(host, path, REGULAR_RESPONSE, response_headers,
                  response_body);
}

void QuicInMemoryCache::AddSpecialResponse(StringPiece host,
                                           StringPiece path,
                                           SpecialResponseType response_type) {
  AddResponseImpl(host, path, response_type, SpdyHeaderBlock(), "");
}

QuicInMemoryCache::QuicInMemoryCache() {}

void QuicInMemoryCache::ResetForTests() {
  STLDeleteValues(&responses_);
}

void QuicInMemoryCache::InitializeFromDirectory(const string& cache_directory) {
  if (cache_directory.empty()) {
    LOG(DFATAL) << "cache_directory must not be empty.";
    return;
  }
  VLOG(1) << "Attempting to initialize QuicInMemoryCache from directory: "
          << cache_directory;
  FilePath directory(cache_directory);
  base::FileEnumerator file_list(directory,
                                 true,
                                 base::FileEnumerator::FILES);

  for (FilePath file_iter = file_list.Next(); !file_iter.empty();
       file_iter = file_list.Next()) {
    BalsaHeaders request_headers, response_headers;
    // Need to skip files in .svn directories
    if (file_iter.value().find("/.svn/") != string::npos) {
      continue;
    }

    // Tease apart filename into host and path.
    StringPiece file(file_iter.value());
    file.remove_prefix(cache_directory.length());
    if (file[0] == '/') {
      file.remove_prefix(1);
    }

    string file_contents;
    base::ReadFileToString(file_iter, &file_contents);

    // Frame HTTP.
    CachingBalsaVisitor caching_visitor;
    BalsaFrame framer;
    framer.set_balsa_headers(&response_headers);
    framer.set_balsa_visitor(&caching_visitor);
    size_t processed = 0;
    while (processed < file_contents.length() &&
           !caching_visitor.done_framing()) {
      processed += framer.ProcessInput(file_contents.c_str() + processed,
                                       file_contents.length() - processed);
    }

    if (!caching_visitor.done_framing()) {
      LOG(DFATAL) << "Did not frame entire message from file: " << file
                  << " (" << processed << " of " << file_contents.length()
                  << " bytes).";
    }
    if (processed < file_contents.length()) {
      // Didn't frame whole file. Assume remainder is body.
      // This sometimes happens as a result of incompatibilities between
      // BalsaFramer and wget's serialization of HTTP sans content-length.
      caching_visitor.AppendToBody(file_contents.c_str() + processed,
                                   file_contents.length() - processed);
      processed += file_contents.length();
    }

    StringPiece base = file;
    if (response_headers.HasHeader("X-Original-Url")) {
      base = response_headers.GetHeader("X-Original-Url");
      response_headers.RemoveAllOfHeader("X-Original-Url");
      // Remove the protocol so that the string is of the form host + path,
      // which is parsed properly below.
      if (StringPieceUtils::StartsWithIgnoreCase(base, "https://")) {
        base.remove_prefix(8);
      } else if (StringPieceUtils::StartsWithIgnoreCase(base, "http://")) {
        base.remove_prefix(7);
      }
    }
    int path_start = base.find_first_of('/');
    DCHECK_LT(0, path_start);
    StringPiece host(base.substr(0, path_start));
    StringPiece path(base.substr(path_start));
    if (path[path.length() - 1] == ',') {
      path.remove_suffix(1);
    }
    AddResponse(host, path,
                SpdyUtils::ResponseHeadersToSpdyHeaders(response_headers),
                caching_visitor.body());
  }
}

QuicInMemoryCache::~QuicInMemoryCache() {
  STLDeleteValues(&responses_);
}

void QuicInMemoryCache::AddResponseImpl(
    StringPiece host,
    StringPiece path,
    SpecialResponseType response_type,
    const SpdyHeaderBlock& response_headers,
    StringPiece response_body) {
  string key = GetKey(host, path);
  VLOG(1) << "Adding response for: " << key;
  if (ContainsKey(responses_, key)) {
    LOG(DFATAL) << "Response for '" << key << "' already exists!";
    return;
  }
  Response* new_response = new Response();
  new_response->set_response_type(response_type);
  new_response->set_headers(response_headers);
  new_response->set_body(response_body);
  responses_[key] = new_response;
}

string QuicInMemoryCache::GetKey(StringPiece host, StringPiece path) const {
  return host.as_string() + path.as_string();
}

}  // namespace tools
}  // namespace net
