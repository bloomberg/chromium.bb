// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/common/appcache/appcache_interfaces.h"

#include <set>

#include "base/lazy_instance.h"
#include "base/strings/string_util.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

namespace {

base::LazyInstance<std::set<std::string> >::Leaky g_supported_schemes =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace appcache {

const char kHttpScheme[] = "http";
const char kHttpsScheme[] = "https";
const char kHttpGETMethod[] = "GET";
const char kHttpHEADMethod[] = "HEAD";

const char kEnableExecutableHandlers[] = "enable-appcache-executable-handlers";

const base::FilePath::CharType kAppCacheDatabaseName[] =
    FILE_PATH_LITERAL("Index");

AppCacheInfo::AppCacheInfo()
    : cache_id(kNoCacheId),
      group_id(0),
      status(UNCACHED),
      size(0),
      is_complete(false) {
}

AppCacheInfo::~AppCacheInfo() {
}

AppCacheResourceInfo::AppCacheResourceInfo()
    : url(),
      size(0),
      is_master(false),
      is_manifest(false),
      is_intercept(false),
      is_fallback(false),
      is_foreign(false),
      is_explicit(false),
      response_id(kNoResponseId) {
}

AppCacheResourceInfo::~AppCacheResourceInfo() {
}

Namespace::Namespace()
    : type(FALLBACK_NAMESPACE),
      is_pattern(false),
      is_executable(false) {
}

Namespace::Namespace(
    NamespaceType type, const GURL& url, const GURL& target, bool is_pattern)
    : type(type),
      namespace_url(url),
      target_url(target),
      is_pattern(is_pattern),
      is_executable(false) {
}

Namespace::Namespace(
    NamespaceType type, const GURL& url, const GURL& target,
    bool is_pattern, bool is_executable)
    : type(type),
      namespace_url(url),
      target_url(target),
      is_pattern(is_pattern),
      is_executable(is_executable) {
}

Namespace::~Namespace() {
}

bool Namespace::IsMatch(const GURL& url) const {
  if (is_pattern) {
    // We have to escape '?' characters since MatchPattern also treats those
    // as wildcards which we don't want here, we only do '*'s.
    std::string pattern = namespace_url.spec();
    if (namespace_url.has_query())
      ReplaceSubstringsAfterOffset(&pattern, 0, "?", "\\?");
    return MatchPattern(url.spec(), pattern);
  }
  return StartsWithASCII(url.spec(), namespace_url.spec(), true);
}

void AddSupportedScheme(const char* scheme) {
  g_supported_schemes.Get().insert(scheme);
}

bool IsSchemeSupported(const GURL& url) {
  bool supported = url.SchemeIs(kHttpScheme) || url.SchemeIs(kHttpsScheme) ||
      (!(g_supported_schemes == NULL) &&
       g_supported_schemes.Get().find(url.scheme()) !=
           g_supported_schemes.Get().end());

#ifndef NDEBUG
  // TODO(michaeln): It would be really nice if this could optionally work for
  // file and filesystem urls too to help web developers experiment and test
  // their apps, perhaps enabled via a cmd line flag or some other developer
  // tool setting.  Unfortunately file scheme net::URLRequests don't produce the
  // same signalling (200 response codes, headers) as http URLRequests, so this
  // doesn't work just yet.
  // supported |= url.SchemeIsFile();
#endif
  return supported;
}

bool IsMethodSupported(const std::string& method) {
  return (method == kHttpGETMethod) || (method == kHttpHEADMethod);
}

bool IsSchemeAndMethodSupported(const net::URLRequest* request) {
  return IsSchemeSupported(request->url()) &&
         IsMethodSupported(request->method());
}

}  // namespace appcache
