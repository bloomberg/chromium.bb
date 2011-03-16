// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "webkit/appcache/view_appcache_internals_job.h"

#include "base/base64.h"
#include "base/logging.h"
#include "base/format_macros.h"
#include "base/utf_string_conversions.h"
#include "base/i18n/time_formatting.h"
#include "base/string_util.h"
#include "net/base/escape.h"
#include "net/url_request/url_request.h"
#include "webkit/appcache/appcache_policy.h"
#include "webkit/appcache/appcache_service.h"

namespace {

const char kErrorMessage[] = "Error in retrieving Application Caches.";
const char kEmptyAppCachesMessage[] = "No available Application Caches.";
const char kRemoveAppCache[] = "Remove this AppCache";
const char kManifest[] = "Manifest: ";
const char kSize[] = "Size: ";
const char kCreationTime[] = "Creation Time: ";
const char kLastAccessTime[] = "Last Access Time: ";
const char kLastUpdateTime[] = "Last Update Time: ";
const char kFormattedDisabledAppCacheMsg[] =
    "<b><i><font color=\"FF0000\">"
    "This Application Cache is disabled by policy.</font></i></b><br/>";

void StartHTML(std::string* out) {
  DCHECK(out);
  out->append(
      "<!DOCTYPE HTML>"
      "<html><title>AppCache Internals</title>"
      "<style>"
      "body { font-family: sans-serif; font-size: 0.8em; }\n"
      "tt, code, pre { font-family: WebKitHack, monospace; }\n"
      ".subsection_body { margin: 10px 0 10px 2em; }\n"
      ".subsection_title { font-weight: bold; }\n"
      "</style>"
      "<script>\n"
      // Unfortunately we can't do XHR from chrome://appcache-internals
      // because the chrome:// protocol restricts access.
      //
      // So instead, we will send commands by doing a form
      // submission (which as a side effect will reload the page).
      "function RemoveCommand(command) {\n"
      "  document.getElementById('cmd').value = command;\n"
      "  document.getElementById('cmdsender').submit();\n"
      "}\n"
      "</script>\n"
      "</head><body>"
      "<form action='' method=GET id=cmdsender>"
      "<input type='hidden' id=cmd name='remove'>"
      "</form>");
}

void EndHTML(std::string* out) {
  DCHECK(out);
  out->append("</body></html>");
}

// Appends an input button to |data| with text |title| that sends the command
// string |command| back to the browser, and then refreshes the page.
void DrawCommandButton(const std::string& title,
                       const std::string& command,
                       std::string* data) {
  base::StringAppendF(data, "<input type=\"button\" value=\"%s\" "
                      "onclick=\"RemoveCommand('%s')\" />",
                      title.c_str(),
                      command.c_str());
}

void AddLiTag(const std::string& element_title,
              const std::string& element_data, std::string* out) {
  DCHECK(out);
  out->append("<li>");
  out->append(element_title);
  out->append(element_data);
  out->append("</li>");
}

void WrapInHREF(const std::string& in, std::string* out) {
  out->append("<a href=");
  out->append(in);
  out->append(">");
  out->append(in);
  out->append("</a><br/>");
}

void AddHTMLFromAppCacheToOutput(
    const appcache::AppCacheService& appcache_service,
    const appcache::AppCacheInfoVector& appcaches, std::string* out) {
  for (std::vector<appcache::AppCacheInfo>::const_iterator info =
           appcaches.begin();
       info != appcaches.end(); ++info) {
    std::string manifest_url_base64;
    base::Base64Encode(info->manifest_url.spec(), &manifest_url_base64);

    out->append("<p>");
    std::string anchored_manifest;
    WrapInHREF(info->manifest_url.spec(), &anchored_manifest);
    out->append(kManifest);
    out->append(anchored_manifest);
    if (!appcache_service.appcache_policy()->CanLoadAppCache(
            info->manifest_url)) {
      out->append(kFormattedDisabledAppCacheMsg);
    }
    out->append("<br/>");
    DrawCommandButton(kRemoveAppCache, manifest_url_base64, out);
    out->append("<ul>");

    AddLiTag(kSize,
             UTF16ToUTF8(FormatBytes(
                 info->size, GetByteDisplayUnits(info->size), true)),
             out);
    AddLiTag(kCreationTime,
             UTF16ToUTF8(TimeFormatFriendlyDateAndTime(info->creation_time)),
             out);
    AddLiTag(kLastAccessTime,
             UTF16ToUTF8(TimeFormatFriendlyDateAndTime(info->last_access_time)),
             out);
    AddLiTag(kLastUpdateTime,
             UTF16ToUTF8(TimeFormatFriendlyDateAndTime(info->last_update_time)),
             out);

    out->append("</ul></p></br>");
  }
}

std::string GetAppCacheManifestToRemove(const std::string& query) {
  if (!StartsWithASCII(query, "remove=", true)) {
    // Not a recognized format.
    return std::string();
  }
  std::string manifest_url_base64 = UnescapeURLComponent(
      query.substr(strlen("remove=")),
      UnescapeRule::NORMAL | UnescapeRule::URL_SPECIAL_CHARS);
  std::string manifest_url;
  base::Base64Decode(manifest_url_base64, &manifest_url);
  return manifest_url;
}

struct ManifestURLComparator {
 public:
  bool operator() (
      const appcache::AppCacheInfo& lhs,
      const appcache::AppCacheInfo& rhs) const {
    return (lhs.manifest_url.spec() < rhs.manifest_url.spec());
  }
} manifest_url_comparator;

}  // namespace

namespace appcache {

ViewAppCacheInternalsJob::ViewAppCacheInternalsJob(
    net::URLRequest* request,
    AppCacheService* service)
    : net::URLRequestSimpleJob(request),
      appcache_service_(service) {
}

ViewAppCacheInternalsJob::~ViewAppCacheInternalsJob() {
  // Cancel callback if job is destroyed before callback is called.
  if (appcache_done_callback_)
    appcache_done_callback_.release()->Cancel();
}

void ViewAppCacheInternalsJob::GetAppCacheInfoAsync() {
  info_collection_ = new AppCacheInfoCollection;
  appcache_done_callback_ =
      new net::CancelableCompletionCallback<ViewAppCacheInternalsJob>(
          this, &ViewAppCacheInternalsJob::AppCacheDone);
  appcache_service_->GetAllAppCacheInfo(
      info_collection_, appcache_done_callback_);
}

void ViewAppCacheInternalsJob::RemoveAppCacheInfoAsync(
    const std::string& manifest_url_spec) {
  appcache_done_callback_ =
      new net::CancelableCompletionCallback<ViewAppCacheInternalsJob>(
          this, &ViewAppCacheInternalsJob::AppCacheDone);

  GURL manifest(manifest_url_spec);
  appcache_service_->DeleteAppCacheGroup(
      manifest, appcache_done_callback_);
}

void ViewAppCacheInternalsJob::Start() {
  if (!request_)
    return;

  // Handle any remove appcache request, then redirect back to
  // the same URL stripped of query parameters. The redirect happens as part
  // of IsRedirectResponse().
  if (request_->url().has_query()) {
    std::string remove_appcache_manifest(
        GetAppCacheManifestToRemove(request_->url().query()));

    // Empty manifests are dealt with by the deleter.
    RemoveAppCacheInfoAsync(remove_appcache_manifest);
  } else {
    GetAppCacheInfoAsync();
  }
}

bool ViewAppCacheInternalsJob::IsRedirectResponse(
    GURL* location, int* http_status_code) {
  if (request_->url().has_query()) {
    // Strip the query parameters.
    GURL::Replacements replacements;
    replacements.ClearQuery();
    *location = request_->url().ReplaceComponents(replacements);
    *http_status_code = 307;
    return true;
  }
  return false;
}

void ViewAppCacheInternalsJob::GenerateHTMLAppCacheInfo(
    std::string* out) const {
  typedef std::map<GURL, AppCacheInfoVector> InfoByOrigin;
  AppCacheInfoVector appcaches;
  for (InfoByOrigin::const_iterator origin =
           info_collection_->infos_by_origin.begin();
       origin != info_collection_->infos_by_origin.end(); ++origin) {
    for (AppCacheInfoVector::const_iterator info =
             origin->second.begin(); info != origin->second.end(); ++info)
      appcaches.push_back(*info);
  }

  std::sort(appcaches.begin(), appcaches.end(), manifest_url_comparator);

  AddHTMLFromAppCacheToOutput(*appcache_service_, appcaches, out);
}

void ViewAppCacheInternalsJob::AppCacheDone(int rv) {
  appcache_done_callback_ = NULL;
  if (rv != net::OK)
    info_collection_ = NULL;
  StartAsync();
}

bool ViewAppCacheInternalsJob::GetData(std::string* mime_type,
                                       std::string* charset,
                                       std::string* data) const {
  mime_type->assign("text/html");
  charset->assign("UTF-8");

  data->clear();
  StartHTML(data);
  if (!info_collection_.get())
    data->append(kErrorMessage);
  else if (info_collection_->infos_by_origin.empty())
    data->append(kEmptyAppCachesMessage);
  else
    GenerateHTMLAppCacheInfo(data);
  EndHTML(data);
  return true;
}

}  // namespace appcache
