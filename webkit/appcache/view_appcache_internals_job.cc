// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/view_appcache_internals_job.h"

#include "base/logging.h"
#include "base/format_macros.h"
#include "base/utf_string_conversions.h"
#include "base/i18n/time_formatting.h"
#include "base/string_util.h"
#include "webkit/appcache/appcache_service.h"

namespace {

const char kErrorMessage[] = "Error in retrieving Application caches.";
const char kEmptyAppCachesMessage[] = "No available Application caches.";

void StartHTML(std::string* out) {
  DCHECK(out);
  out->append(
      "<!DOCTYPE HTML>"
      "<html><head><title>AppCache Internals</title>"
      "<style>"
      "body { font-family: sans-serif; font-size: 0.8em; }\n"
      "tt, code, pre { font-family: WebKitHack, monospace; }\n"
      ".subsection_body { margin: 10px 0 10px 2em; }\n"
      ".subsection_title { font-weight: bold; }\n"
      "</style>");
}

void EndHTML(std::string* out) {
  DCHECK(out);
  out->append("</body></html>");
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
  out->append("<a href='");
  out->append(in);
  out->append("'>");
  out->append(in);
  out->append("</a><br/>");
}

void AddHTMLFromAppCacheToOutput(
    const appcache::AppCacheInfoVector& appcaches, std::string* out) {
  for (std::vector<appcache::AppCacheInfo>::const_iterator info =
           appcaches.begin();
       info != appcaches.end(); ++info) {
    out->append("<p>");
    std::string anchored_manifest;
    WrapInHREF(info->manifest_url.spec(), &anchored_manifest);
    out->append("Manifest: ");
    out->append(anchored_manifest);
    out->append("<ul>");

    AddLiTag("Size: ",
             WideToUTF8(FormatBytes(
                 info->size, GetByteDisplayUnits(info->size), true)),
             out);
    AddLiTag("Creation Time: ",
             WideToUTF8(TimeFormatFriendlyDateAndTime(
                 info->creation_time)),
             out);
    AddLiTag("Last Access Time: ",
             WideToUTF8(
                 TimeFormatFriendlyDateAndTime(info->last_access_time)),
             out);
    AddLiTag("Last Update Time: ",
             WideToUTF8(TimeFormatFriendlyDateAndTime(info->last_update_time)),
             out);

    out->append("</ul></p></br>");
  }
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
    URLRequest* request,
    AppCacheService* service) : URLRequestSimpleJob(request),
                                appcache_service_(service) {
}

ViewAppCacheInternalsJob::~ViewAppCacheInternalsJob() {
  // Cancel callback if job is destroyed before callback is called.
  if (appcache_info_callback_)
    appcache_info_callback_.release()->Cancel();
}

void ViewAppCacheInternalsJob::Start() {
  if (!request_)
    return;

  info_collection_ = new AppCacheInfoCollection;
  appcache_info_callback_ =
      new net::CancelableCompletionCallback<ViewAppCacheInternalsJob>(
          this, &ViewAppCacheInternalsJob::OnGotAppCacheInfo);

  appcache_service_->GetAllAppCacheInfo(
      info_collection_, appcache_info_callback_);
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

  AddHTMLFromAppCacheToOutput(appcaches, out);
}

void ViewAppCacheInternalsJob::OnGotAppCacheInfo(int rv) {
  appcache_info_callback_ = NULL;
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
