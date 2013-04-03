// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache_interfaces.h"

#include <set>

#include "base/lazy_instance.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebApplicationCacheHost.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebConsoleMessage.h"

using WebKit::WebApplicationCacheHost;
using WebKit::WebConsoleMessage;

namespace {

base::LazyInstance<std::set<std::string> >::Leaky g_supported_schemes =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace appcache {

const char kHttpScheme[] = "http";
const char kHttpsScheme[] = "https";
const char kHttpGETMethod[] = "GET";
const char kHttpHEADMethod[] = "HEAD";

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
      is_pattern(false) {
}

Namespace::Namespace(
    NamespaceType type, const GURL& url, const GURL& target, bool is_pattern)
    : type(type),
      namespace_url(url),
      target_url(target),
      is_pattern(is_pattern) {
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

// Ensure that enum values never get out of sync with the
// ones declared for use within the WebKit api
COMPILE_ASSERT((int)WebApplicationCacheHost::Uncached ==
               (int)UNCACHED, Uncached);
COMPILE_ASSERT((int)WebApplicationCacheHost::Idle ==
               (int)IDLE, Idle);
COMPILE_ASSERT((int)WebApplicationCacheHost::Checking ==
               (int)CHECKING, Checking);
COMPILE_ASSERT((int)WebApplicationCacheHost::Downloading ==
               (int)DOWNLOADING, Downloading);
COMPILE_ASSERT((int)WebApplicationCacheHost::UpdateReady ==
               (int)UPDATE_READY, UpdateReady);
COMPILE_ASSERT((int)WebApplicationCacheHost::Obsolete ==
               (int)OBSOLETE, Obsolete);
COMPILE_ASSERT((int)WebApplicationCacheHost::CheckingEvent ==
               (int)CHECKING_EVENT, CheckingEvent);
COMPILE_ASSERT((int)WebApplicationCacheHost::ErrorEvent ==
               (int)ERROR_EVENT, ErrorEvent);
COMPILE_ASSERT((int)WebApplicationCacheHost::NoUpdateEvent ==
               (int)NO_UPDATE_EVENT, NoUpdateEvent);
COMPILE_ASSERT((int)WebApplicationCacheHost::DownloadingEvent ==
               (int)DOWNLOADING_EVENT, DownloadingEvent);
COMPILE_ASSERT((int)WebApplicationCacheHost::ProgressEvent ==
               (int)PROGRESS_EVENT, ProgressEvent);
COMPILE_ASSERT((int)WebApplicationCacheHost::UpdateReadyEvent ==
               (int)UPDATE_READY_EVENT, UpdateReadyEvent);
COMPILE_ASSERT((int)WebApplicationCacheHost::CachedEvent ==
               (int)CACHED_EVENT, CachedEvent);
COMPILE_ASSERT((int)WebApplicationCacheHost::ObsoleteEvent ==
               (int)OBSOLETE_EVENT, ObsoleteEvent);
COMPILE_ASSERT((int)WebConsoleMessage::LevelDebug ==
               (int)LOG_DEBUG, LevelDebug);
COMPILE_ASSERT((int)WebConsoleMessage::LevelLog ==
               (int)LOG_INFO, LevelLog);
COMPILE_ASSERT((int)WebConsoleMessage::LevelWarning ==
               (int)LOG_WARNING, LevelWarning);
COMPILE_ASSERT((int)WebConsoleMessage::LevelError ==
               (int)LOG_ERROR, LevelError);

}  // namespace appcache
