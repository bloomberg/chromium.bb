// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_in_memory_cache.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/quic/quic_bug_tracker.h"
#include "net/spdy/spdy_http_utils.h"

using base::FilePath;
using base::IntToString;
using base::StringPiece;
using std::string;

namespace net {

QuicInMemoryCache::ServerPushInfo::ServerPushInfo(
    GURL request_url,
    const SpdyHeaderBlock& headers,
    net::SpdyPriority priority,
    string body)
    : request_url(request_url),
      headers(headers),
      priority(priority),
      body(body) {}

QuicInMemoryCache::Response::Response() : response_type_(REGULAR_RESPONSE) {}

QuicInMemoryCache::Response::~Response() {}

// static
QuicInMemoryCache* QuicInMemoryCache::GetInstance() {
  return base::Singleton<QuicInMemoryCache>::get();
}

const QuicInMemoryCache::Response* QuicInMemoryCache::GetResponse(
    StringPiece host,
    StringPiece path) const {
  ResponseMap::const_iterator it = responses_.find(GetKey(host, path));
  if (it == responses_.end()) {
    if (default_response_.get()) {
      return default_response_.get();
    }
    return nullptr;
  }
  return it->second;
}

typedef QuicInMemoryCache::ServerPushInfo ServerPushInfo;

void QuicInMemoryCache::AddSimpleResponse(StringPiece host,
                                          StringPiece path,
                                          int response_code,
                                          StringPiece body) {
  SpdyHeaderBlock response_headers;
  response_headers[":status"] = IntToString(response_code);
  response_headers["content-length"] =
      IntToString(static_cast<int>(body.length()));
  AddResponse(host, path, response_headers, body);
}

void QuicInMemoryCache::AddSimpleResponseWithServerPushResources(
    StringPiece host,
    StringPiece path,
    int response_code,
    StringPiece body,
    list<ServerPushInfo> push_resources) {
  AddSimpleResponse(host, path, response_code, body);
  MaybeAddServerPushResources(host, path, push_resources);
}

void QuicInMemoryCache::AddDefaultResponse(Response* response) {
  default_response_.reset(response);
}

void QuicInMemoryCache::AddResponse(StringPiece host,
                                    StringPiece path,
                                    const SpdyHeaderBlock& response_headers,
                                    StringPiece response_body) {
  AddResponseImpl(host, path, REGULAR_RESPONSE, response_headers, response_body,
                  SpdyHeaderBlock());
}

void QuicInMemoryCache::AddResponse(StringPiece host,
                                    StringPiece path,
                                    const SpdyHeaderBlock& response_headers,
                                    StringPiece response_body,
                                    const SpdyHeaderBlock& response_trailers) {
  AddResponseImpl(host, path, REGULAR_RESPONSE, response_headers, response_body,
                  response_trailers);
}

void QuicInMemoryCache::AddSpecialResponse(StringPiece host,
                                           StringPiece path,
                                           SpecialResponseType response_type) {
  AddResponseImpl(host, path, response_type, SpdyHeaderBlock(), "",
                  SpdyHeaderBlock());
}

QuicInMemoryCache::QuicInMemoryCache() {}

void QuicInMemoryCache::ResetForTests() {
  STLDeleteValues(&responses_);
  server_push_resources_.clear();
}

void QuicInMemoryCache::InitializeFromDirectory(const string& cache_directory) {
  if (cache_directory.empty()) {
    QUIC_BUG << "cache_directory must not be empty.";
    return;
  }
  VLOG(1) << "Attempting to initialize QuicInMemoryCache from directory: "
          << cache_directory;
  FilePath directory(FilePath::FromUTF8Unsafe(cache_directory));
  base::FileEnumerator file_list(directory, true, base::FileEnumerator::FILES);

  for (FilePath file_iter = file_list.Next(); !file_iter.empty();
       file_iter = file_list.Next()) {
    // Need to skip files in .svn directories
    if (file_iter.value().find(FILE_PATH_LITERAL("/.svn/")) != string::npos) {
      continue;
    }

    // Tease apart filename into host and path.
    string file = file_iter.AsUTF8Unsafe();
    file.erase(0, cache_directory.length());
    if (file[0] == '/') {
      file.erase(0, 1);
    }

    string file_contents;
    base::ReadFileToString(file_iter, &file_contents);
    int file_len = static_cast<int>(file_contents.length());
    int headers_end =
        HttpUtil::LocateEndOfHeaders(file_contents.data(), file_len);
    if (headers_end < 1) {
      LOG(DFATAL) << "Headers invalid or empty, ignoring: " << file;
      continue;
    }

    string raw_headers =
        HttpUtil::AssembleRawHeaders(file_contents.data(), headers_end);

    scoped_refptr<HttpResponseHeaders> response_headers =
        new HttpResponseHeaders(raw_headers);

    string base;
    if (response_headers->GetNormalizedHeader("X-Original-Url", &base)) {
      response_headers->RemoveHeader("X-Original-Url");
      // Remove the protocol so we can add it below.
      if (base::StartsWith(base, "https://",
                           base::CompareCase::INSENSITIVE_ASCII)) {
        base = base.substr(8);
      } else if (base::StartsWith(base, "http://",
                                  base::CompareCase::INSENSITIVE_ASCII)) {
        base = base.substr(7);
      }
    } else {
      base = file;
    }

    size_t path_start = base.find_first_of('/');
    StringPiece host(StringPiece(base).substr(0, path_start));
    StringPiece path(StringPiece(base).substr(path_start));
    if (path[path.length() - 1] == ',') {
      path.remove_suffix(1);
    }
    StringPiece body(file_contents.data() + headers_end,
                     file_contents.size() - headers_end);
    SpdyHeaderBlock header_block;
    CreateSpdyHeadersFromHttpResponse(*response_headers, HTTP2, &header_block);
    AddResponse(host, path, header_block, body);
  }
}

list<ServerPushInfo> QuicInMemoryCache::GetServerPushResources(
    string request_url) {
  list<ServerPushInfo> resources;
  auto resource_range = server_push_resources_.equal_range(request_url);
  for (auto it = resource_range.first; it != resource_range.second; ++it) {
    resources.push_back(it->second);
  }
  return resources;
}

QuicInMemoryCache::~QuicInMemoryCache() {
  STLDeleteValues(&responses_);
}

void QuicInMemoryCache::AddResponseImpl(
    StringPiece host,
    StringPiece path,
    SpecialResponseType response_type,
    const SpdyHeaderBlock& response_headers,
    StringPiece response_body,
    const SpdyHeaderBlock& response_trailers) {
  DCHECK(!host.empty()) << "Host must be populated, e.g. \"www.google.com\"";
  string key = GetKey(host, path);
  if (ContainsKey(responses_, key)) {
    QUIC_BUG << "Response for '" << key << "' already exists!";
    return;
  }
  Response* new_response = new Response();
  new_response->set_response_type(response_type);
  new_response->set_headers(response_headers);
  new_response->set_body(response_body);
  new_response->set_trailers(response_trailers);
  responses_[key] = new_response;
}

string QuicInMemoryCache::GetKey(StringPiece host, StringPiece path) const {
  return host.as_string() + path.as_string();
}

void QuicInMemoryCache::MaybeAddServerPushResources(
    StringPiece request_host,
    StringPiece request_path,
    list<ServerPushInfo> push_resources) {
  string request_url = request_host.as_string() + request_path.as_string();

  for (const auto& push_resource : push_resources) {
    if (PushResourceExistsInCache(request_url, push_resource)) {
      continue;
    }

    DVLOG(1) << "Add request-resource association.";
    server_push_resources_.insert(std::make_pair(request_url, push_resource));
    string path = push_resource.request_url.path();
    if (responses_.find(GetKey(request_host, path)) == responses_.end()) {
      // Add a server push response to responses map, if it is not in the map.
      SpdyHeaderBlock headers = push_resource.headers;
      StringPiece body = push_resource.body;
      DVLOG(1) << "Add response for push resource " << body;
      AddResponse(request_host, path, headers, body);
    }
  }
}

bool QuicInMemoryCache::PushResourceExistsInCache(string original_request_url,
                                                  ServerPushInfo resource) {
  auto resource_range =
      server_push_resources_.equal_range(original_request_url);
  for (auto it = resource_range.first; it != resource_range.second; ++it) {
    ServerPushInfo push_resource = it->second;
    if (push_resource.request_url.spec() == resource.request_url.spec()) {
      return true;
    }
  }
  return false;
}

}  // namespace net
